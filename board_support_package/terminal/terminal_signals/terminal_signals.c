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


static	char *sDEV = "";
static  char *sDEBUG = "";


SIGNALS_BEGIN(DSPA_SIGNALS_NAME)
	_STRING_R_  ("Ballistic computer board", sDEV, NULL),
		_U32_R_	("Test hardware",	test_hardware_result,&sDEV),
	_STRING_R_  ("Debug section", sDEBUG, NULL),
		_FLOAT_R_("ds1621s+(°C) ", current_temp, &sDEBUG),
SIGNALS_END(DSPA_SIGNALS_NAME)


int	init_terminal_signals(void){
	return	SIG_INIT(DSPA_SIGNALS_NAME);
}


void signal_change_handler(void *s) {

}
