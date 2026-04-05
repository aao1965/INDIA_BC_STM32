/*
 * terminal_signals.c
 *
 *  Created on: Oct 6, 2020
 *      Author: Auramenka Andrew
 */

#include	"dspa.h"
#include	"dspa_defs.h"
#include 	"dspa_sigdefs.h"
#include	"terminal_signals.h"

#include 	"am1805.h"
#include 	"ds1621.h"
#include	"fm22l16.h"
#include 	"led_blink.h"

static	char *sDEV = "";
static	char *sFLASH = "";
static 	char *sFM22L16 = "";
static  char *sDEBUG = "";
static 	char *sTIME = "";
static  char *sTIME_CORRECTION = "";

static	AM1805_Time_t set_time={
	    .year     = 2026,
	    .month    = 1,
	    .day      = 1,
	    .hours    = 0,
	    .minutes  = 0,
	    .seconds  = 0,
	    .hundredths = 0,
	    .is_xt_active = true  /* По умолчанию используем кварц */
	};

static	bool	run_set_data=	false;
static	float  	ppm_rtc= 0;
static	bool	run_set_correction=	false;
static  bool	reset_1805= false;
static 	bool	run_fm22l16_full_test= false;
static  bool 	status_fm22l16_test= false;

SIGNALS_BEGIN(DSPA_SIGNALS_NAME)
	_STRING_R_  ("Ballistic computer board", sDEV, NULL),
		_U32_R_	("Test hardware",	test_hardware_result,&sDEV),
		_STRING_R_ ("Date + Time", sTIME, &sDEV),
			_STRING_R_ ("current data ", time_str, &sTIME),
			_BOOL_R_("XT_osc present",set_time.is_xt_active, &sTIME),
			_BYTE_RW_("hours", set_time.hours, &sTIME),
			_BYTE_RW_("minutes", set_time.minutes, &sTIME),
			_BYTE_RW_("seconds", set_time.seconds, &sTIME),
			_U16_RW_("year",set_time.year, &sTIME),
			_BYTE_RW_("month", set_time.month, &sTIME),
			_BYTE_RW_("day", set_time.day, &sTIME),
			_BOOL_RW_("set data",run_set_data, &sTIME),
			_STRING_R_ ("Time calibration", sTIME_CORRECTION, &sTIME),
				_FLOAT_RW_("corr.[sec./hour]('+' if clock is fast)",ppm_rtc,&sTIME_CORRECTION ),
				_BOOL_RW_("set correction",run_set_correction, &sTIME_CORRECTION),
				_BYTE_R_("BREF",  rtc_diag.bref, &sTIME_CORRECTION),
				_BYTE_R_("CAL_XT",  rtc_diag.cal_xt, &sTIME_CORRECTION),
				_BYTE_R_("CTRL1",  rtc_diag.ctrl1, &sTIME_CORRECTION),
				_BYTE_R_("OSC CTRL",  rtc_diag.osc_ctrl, &sTIME_CORRECTION),
				_BYTE_R_("OSC STATUS",  rtc_diag.osc_stat, &sTIME_CORRECTION),
				_BYTE_R_("STATUS",  rtc_diag.status, &sTIME_CORRECTION),
				_BOOL_RW_("reset AM1805",reset_1805, &sTIME_CORRECTION),
	_STRING_R_  ("Debug section", sDEBUG, NULL),
		_STRING_R_("W25Q16JVSSIQ", sFLASH, &sDEBUG),
			_BYTE_R_("JDECID0", DD15.manufacturer_id,&sFLASH),
			_BYTE_R_("JDECID1", DD15.memory_type,&sFLASH),
			_BYTE_R_("JDECID2", DD15.capacity_id,&sFLASH),
			_U64_R_("DD2 UID", 	DD15.unique_id,&sFLASH),
		_STRING_R_("FM22L16", sFM22L16, &sDEBUG),
			_BOOL_R_("status test", status_fm22l16_test, &sFM22L16),
			_BOOL_RW_("full test(all data will be destroyed)", run_fm22l16_full_test, &sFM22L16),
		_FLOAT_R_("ds1621s+(°C) ", current_temp, &sDEBUG),
		_U16_R_("fsmc cnt ", test_fpga, &sDEBUG),
		_U32_R_("1 kHz interrupt", cnt_1kHz_interrupt, &sDEBUG)

SIGNALS_END(DSPA_SIGNALS_NAME)


int	init_terminal_signals(void){
	return	SIG_INIT(DSPA_SIGNALS_NAME);
}


void signal_change_handler(void *s) {
	// установка даты
	if (s==&run_set_data){
		run_set_data= false;
		AM1805_SetTime(&set_time);

	}
	// запись коррекции RTC
	if (s==&run_set_correction){
		run_set_correction=	false;
		AM1805_SetCalibrationByDrift(ppm_rtc);
	}

	// reset 1805
	if (s== &reset_1805){
		reset_1805= false;

		AM1805_ForceInit(&hi2c1, DS1621_GetI2CMutex(), 0x55AA3C0F);
		AM1805_GetDiagnostics(&rtc_diag);
	}

	// полный тест
	if (s == &run_fm22l16_full_test) {
		run_fm22l16_full_test = false;
		status_fm22l16_test= false;
		status_fm22l16_test = (fm22_test_full() == FSMC_OK);
	}
}
