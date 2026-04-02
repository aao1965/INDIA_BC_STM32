#include	"dspa.h"
#include	"dspa_defs.h"
#include 	"dspa_sigdefs.h"

#include	"board_support_package.h"
#include 	"terminal.h"
#include	"std_com.h"
#include	"wrapper.h"
#include 	"version_control.h"
#include 	"w25q16.h"
#include 	"pin_mgmt.h"
#include 	"rgb_led.h"
#include 	"led_blink.h"
#include	"terminal_signals.h"

/* 	main terminal uart */
Uart_ctrl_t	uctrl;

/* dspa  main func. */
static	PROG_Config_t	PROG_Config;
static	SYS_Config_t  	SYS_Config;

static 	bool reset_request=	false;
static 	int	handle;

static	char* uint32_to_string(uint32_t n, char* buffer, size_t buffer_size) ;



//	terminal communication functions
static int	Send(du8 *const pbuf, const unsigned int num);
static int	InBufUsed(void);
static int	GetData(du8 *const pbuf, const unsigned int num);
static void	FlushInBuffer(void);

//	main device functions
static dboolean	sys_SaveSettings(void);
static void	sys_Reset(void);
static char*	sys_SelfTest(void);

//	main	programming functions
static du32		prg_ReadId(const du8 dev_num);
static dboolean prg_Read(const du8 dev_num, du8 *const pusData,	const du32 ulStartAddress);
static dboolean prg_Erase(const du8 dev_num);
static dboolean prg_Write(const du8 dev_num, du8 *const pusData, const du32 ulStartAddress);
static void		prg_Reset(const du8 dev_num);
static dboolean prg_Finish(const du8 dev_num);


/* device descriptod */
Dev_descriptor_t dev_descr_arr[_ID_DEV_NUM] =
{

	{
	//	FLASH
		"DD4-W25Q16JV/M_ID=",
		W25Q16_PAGE_SIZE,
		W25Q16_PAGE_SIZE,
		//DSPA_CFG_READ_PAGE_SIZE,
		_TERMINAL_FPFA_MAX_NUM_BLOCK_*W25Q16_BLOCK_SIZE,
		W25Q16_TIMEOUT_CHIP_WRITE_S,
		W25Q16_TIMEOUT_CHIP_ERASE_S,
		dtrue,
		0,
		0
		}
};


/*	Telemetry descriptor */
TEL_Descriptor_t TEL_dscr = {
		100/*us*/* (_TERMINAL_TASK_TICK_ * 1000),	// Period (lsb = 0.01 us)
		TEL_MAX_SIGNALS, 		// maximum number signals permitted to use in telemetry
		0,  									// Frame size (not supportes)
		TEL_ATTR_SUPPORT_BUFFER					//attributes
		};




/*	Initialization dspa  system */
bool terminal_init(void) {

	PROG_Config.descriptor = dev_descr_arr;
	PROG_Config.descriptor_size = _ID_DEV_NUM;

	PROG_Config.FuncPtr_ReadId = &prg_ReadId;
	PROG_Config.FuncPtr_Read = &prg_Read;
	PROG_Config.FuncPtr_Erase = &prg_Erase;
	PROG_Config.FuncPtr_Write = &prg_Write;
	PROG_Config.FuncPtr_Reset = &prg_Reset;
	PROG_Config.Func_Ptr_Finish = &prg_Finish;

	SYS_Config.FuncPtr_SaveSettings = &sys_SaveSettings;
	SYS_Config.FuncPtr_SelfTest = &sys_SelfTest;
	SYS_Config.FuncPtr_SysReset = &sys_Reset;

	SYS_Config.system_info = device_info_create();

	SYS_Init(&SYS_Config);

	PROG_Init(&PROG_Config);

	TEL_Set_descriptor(&TEL_dscr);

	dspa_dispatcher_init(Send, GetData, InBufUsed, FlushInBuffer);

	handle = init_terminal_signals();

	/*	 uart initialization	*/

	uctrl.huart = &huart1;
	uctrl.dma_handle_rx = &hdma_usart1_rx;
	uctrl.dma_handle_tx = &hdma_usart1_tx;

	uctrl.rx_buf_size = _TERMINAL_RX_BUFF_SIZE_;
	uctrl.tx_buf_size = _TERMINAL_TX_BUFF_SIZE_;
	com_init(&uctrl);

#ifdef	_TERMINAL_PERMISSION_WRAPPER_
	wrp_init(&uctrl, _TERMINAL_WRAPPER_ADDRESS_, _TERMINAL_WRAPPER_ALT_ADDRESS_,
			_TERMINAL_RECEIVE_TIMEOUT_US_);
#endif

	set_state_led(LED_STATE_BLINK_WHITE);

	com_start_receive(&uctrl);
	return true;

}


/*
 *	Handling DSPAssist Task
 */
void terminal_task(void) {

	terminal_telemetry_handler();

	dspa_dispatcher(_TERMINAL_TASK_TICK_ * 1000);

#ifdef	_TERMINAL_PERMISSION_WRAPPER_
	com_check_timeout(&uctrl, _TERMINAL_TASK_TICK_ * 1000);
#endif

	if (reset_request) {
		osDelay(200);
		bsp_system_reset();
	}

	led_blink();

	osDelay(_TERMINAL_TASK_TICK_);
}

/* telemetry */
void	terminal_telemetry_handler(void){
	TEL_Sample_Update(handle);			// Telemetry: update sample value
	TEL_Sample_Save();					// Telemetry: save sample to buffer
}

/**************************************************************************************************
 *  terminal communication functionS
 *************************************************************************************************/
//	Translate data -> UART
static int Send(unsigned char *const buf, const unsigned int num) {

	if (get_state_led()==LED_STATE_BLINK_WHITE){
	//	set_state_led(LED_STATE_BREATH_WHITE);	//	LED_STATE_RG_OFF
		set_state_led(LED_STATE_RG_OFF);	//
	}
	com_send(&uctrl, buf, num);
	return num;
}

//	Copy ingoing dataS -> buffer 'buf'
static int GetData(unsigned char *const buf, const unsigned int num) {

	unsigned int num_i = num;
	if (num_i == 0){
		num_i = com_inbuf_used(&uctrl);
	}
	if (num_i == 0){
		return 0;
	}
	com_inbuf_fetch(&uctrl, buf, num_i);

	return num_i;
}



//	How many data item
static int InBufUsed(void){
	return com_inbuf_used(&uctrl);
}

//	Reset input buffers
static void FlushInBuffer(void){
	com_flush_in_buffer(&uctrl);
}

/**************************************************************************************************
 *	Main device functionS
 *	Maintaining the basic settings
 **************************************************************************************************/
static dboolean sys_SaveSettings(void){
	bool	result=	dtrue;
	return	result;
}

//	Device Reset
static void sys_Reset(void){
    reset_request = true;
}

//	Self test
static char* sys_SelfTest(void){
	static char result[64];
	char str[16];
	uint32_to_string(test_hardware_result, str, sizeof(str));
	snprintf(result, sizeof(result), "Self-diagnosis: Device status(0=ok): %s", str);
	return result;
}


/**************************************************************************************************
 *  main programming functions
 *************************************************************************************************/
//	read ID
static du32 prg_ReadId(const du8 dev_num) {
	uint32_t t=0;

	switch (dev_num) {
	case _ID_FPGA_FLASH:{
		t=	(uint32_t)DD15.manufacturer_id;
		break;
	}
	default:	break;
	}

	return t;
}

static dboolean prg_Read(const du8 dev_num, du8 *const pusData,	const du32 ulStartAddress) {
	dboolean result = dfalse;

	switch (dev_num) {
	case _ID_FPGA_FLASH:{
		SPI_Bus_Acquire_For_STM32();
		result=	(W25Q16_ReadData(&DD15, ulStartAddress, pusData, W25Q16_PAGE_SIZE)==osOK);
		break;
	}
	default:	break;
	}
	return result;
}


//	erase all memory
static dboolean prg_Erase(const du8 dev_num) {
	dboolean result=	false;

	set_state_led(LED_STATE_POLICE_FLASHER);

	switch (dev_num) {
	case _ID_FPGA_FLASH: {
		SPI_Bus_Acquire_For_STM32();
		for (uint32_t i = 0; i < _TERMINAL_FPFA_MAX_NUM_BLOCK_; i++) {
			if (W25Q16_BlockErase(&DD15, i * W25Q16_BLOCK_SIZE) != osOK) {
				SPI_Bus_Release_To_FPGA();
				return	dfalse;
			}
		}

		result=	dtrue;
		break;
	}
	default:	break;
	}
	return result;
}

//	write page
static dboolean prg_Write(const du8 dev_num, du8 *const pusData,	const du32 ulStartAddress) {
	dboolean result = dfalse;

	switch (dev_num) {
	case _ID_FPGA_FLASH: {
		result =(W25Q16_PageProgram(&DD15, ulStartAddress, pusData, W25Q16_PAGE_SIZE) == osOK);
		break;
	}
	default:	break;
	}
	return result;
}


//	reset
static void prg_Reset(const du8 dev_num) {

	//jump_main_application(SELF_APPL_ADDRESS);
	SPI_Bus_Release_To_FPGA();
	FPGA_Reset();
}



//	Finish programming
static dboolean prg_Finish(const du8 dev_num) {
	dboolean result = dfalse;
	switch (dev_num) {
	case _ID_FPGA_FLASH: {

		set_state_led(LED_STATE_RG_OFF);

		SPI_Bus_Release_To_FPGA();
		FPGA_Reset();
		result=	true;
		break;
	}
	default:
		break;
	}

	return result;
}

/**************************************************************************************************
 *  auxiliary functions
 *************************************************************************************************/

/* uint32_t ->string */
static char* uint32_to_string(uint32_t n, char *buffer, size_t buffer_size) {
	if (buffer_size < 11)
		return NULL;
	if (n == 0) {
		buffer[0] = '0';
		buffer[1] = '\0';
		return buffer;
	}
	char temp[11];
	int i = 0;
	while (n > 0) {
		temp[i++] = '0' + (n % 10);
		n /= 10;
	}
	int j = 0;
	while (i > 0) {
		buffer[j++] = temp[--i];
	}
	buffer[j] = '\0';
	return buffer;
}





