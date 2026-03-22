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
static  char *sDEBUG = "";
static 	char *sTIME = "";

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

SIGNALS_BEGIN(DSPA_SIGNALS_NAME)
	_STRING_R_  ("Ballistic computer board", sDEV, NULL),
		_U32_R_	("Test hardware",	test_hardware_result,&sDEV),
		_STRING_R_ ("Data ", sTIME, &sDEV),
			_STRING_R_ ("current data ", time_str, &sTIME),
			_BYTE_RW_("hours", set_time.hours, &sTIME),
			_BYTE_RW_("minutes", set_time.minutes, &sTIME),
			_BYTE_RW_("seconds", set_time.seconds, &sTIME),
			_U16_RW_("year",set_time.year, &sTIME),
			_BYTE_RW_("month", set_time.month, &sTIME),
			_BYTE_RW_("day", set_time.day, &sTIME),
			_BOOL_RW_("set data",run_set_data, &sTIME),
	_STRING_R_  ("Debug section", sDEBUG, NULL),
		_FLOAT_R_("ds1621s+(°C) ", current_temp, &sDEBUG),
SIGNALS_END(DSPA_SIGNALS_NAME)


int	init_terminal_signals(void){
	return	SIG_INIT(DSPA_SIGNALS_NAME);
}


void signal_change_handler(void *s) {
	if (s==&run_set_data){
		run_set_data= false;
		AM1805_SetTime(&set_time);

	}
}
