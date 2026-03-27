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

static	char *sDEV = "";
static	char *sFLASH = "";
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
static	float  ppm_rtc= 0;
static	bool	run_set_correction=	false;

SIGNALS_BEGIN(DSPA_SIGNALS_NAME)
	_STRING_R_  ("Ballistic computer board", sDEV, NULL),
		_U32_R_	("Test hardware",	test_hardware_result,&sDEV),
		_STRING_R_("W25Q16JVSSIQ", sFLASH, &sDEV),
			_BYTE_R_("JDECID0", DD15.manufacturer_id,&sFLASH),
			_BYTE_R_("JDECID1", DD15.memory_type,&sFLASH),
			_BYTE_R_("JDECID2", DD15.capacity_id,&sFLASH),
			_U64_R_("DD2 UID", 	DD15.unique_id,&sFLASH),

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
				_FLOAT_RW_("corr.[ppm](+speeds up)",ppm_rtc,&sTIME_CORRECTION ),
				_BOOL_RW_("set correction",run_set_correction, &sTIME_CORRECTION),
				_BYTE_R_("BREF",  rtc_diag.bref, &sTIME_CORRECTION),
				_BYTE_R_("CAL_XT",  rtc_diag.cal_xt, &sTIME_CORRECTION),
				_BYTE_R_("CTRL1",  rtc_diag.ctrl1, &sTIME_CORRECTION),
				_BYTE_R_("OSC CTRL",  rtc_diag.osc_ctrl, &sTIME_CORRECTION),
				_BYTE_R_("OSC STATUS",  rtc_diag.osc_stat, &sTIME_CORRECTION),
				_BYTE_R_("STATUS",  rtc_diag.status, &sTIME_CORRECTION),
	_STRING_R_  ("Debug section", sDEBUG, NULL),
		_FLOAT_R_("ds1621s+(°C) ", current_temp, &sDEBUG)

SIGNALS_END(DSPA_SIGNALS_NAME)


int	init_terminal_signals(void){
	return	SIG_INIT(DSPA_SIGNALS_NAME);
}


void signal_change_handler(void *s) {
	if (s==&run_set_data){
		run_set_data= false;
		AM1805_SetTime(&set_time);

	}

	if (s==&run_set_correction){
		run_set_correction=	false;
		AM1805_SetCalibration(ppm_rtc);
	}
}
