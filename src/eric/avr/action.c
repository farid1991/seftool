#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <libserialport.h>

#include <core/common.h>
#include <core/connection.h>
#include <core/serial.h>
#include <core/io_ctx.h>

#include <eric/common/eric_common.h>
#include "eric_t28.h"

int action_avr_identify(struct sp_port *port, struct phone_info *phone)
{
	switch (phone->type) {
	case ERICSSON_T1X:
	case ERICSSON_T28:
		if (eric_t28_identify(port, phone) != 0)
			return -1;
		break;
	default:
		fprintf(stderr, "Not supported (yet)\n");
		return -1;
	}

	return 0;
}

int action_avr_read_eeprom(struct sp_port *port, struct phone_info *phone)
{
	switch (phone->type) {
	case ERICSSON_T1X:
	case ERICSSON_T28:
		if (eric_t28_read_eeprom(port, phone) != 0)
			return -1;
		break;
	default:
		fprintf(stderr, "Not supported (yet)\n");
		return -1;
	}

	return 0;
}

int action_avr_write_eeprom(struct sp_port *port, struct phone_info *phone, const char *eeprom)
{
	switch (phone->type) {
	case ERICSSON_T1X:
	case ERICSSON_T28:
		if (eric_t28_write_eeprom(port, phone, eeprom) != 0)
			return -1;
		break;
	default:
		fprintf(stderr, "Not supported (yet)\n");
		return -1;
	}

	return 0;
}

// int action_avr_unlock_usercode(struct sp_port *port, struct phone_info *phone)
// {
// 	switch (phone->type) {
// 	case ERICSSON_T28:
// 	default:
// 		fprintf(stderr, "Not supported (yet)\n");
// 		return -1;
// 	}

// 	return 0;
// }
