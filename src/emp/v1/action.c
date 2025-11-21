#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <libserialport.h>

#include <core/common.h>
#include <core/serial.h>
#include <core/io_ctx.h>
#include <emp/common/avr_common.h>

#include "emp_avr.h"

int emp_avr_action_flash_sbn(struct sp_port *port, struct phone_info *phone, const char *fname)
{
	if (emp_avr_bootup(port, phone) != 0)
		return -1;
	if (emp_avr_send_flashloader(port, phone) != 0)
		return -1;
	if (emp_avr_flash_sbn(port, fname) != 0)
		return -1;

	emp_avr_finish(port);
	return 0;
}

int emp_avr_action_identify(struct sp_port *port, struct phone_info *phone)
{
	if (emp_avr_bootup(port, phone) != 0)
		return -1;
	if (emp_avr_send_flashloader(port, phone) != 0)
		return -1;
	emp_avr_finish(port);
	return 0;
}
