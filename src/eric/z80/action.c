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
#include "eric_z80.h"
#include "eric_3x8.h"
#include "eric_6xx.h"
#include "eric_8xx.h"

int action_z80_identify(struct sp_port *port, struct phone_info *phone)
{
	switch (phone->type) {
	case ERICSSON_338:
		if (eric_3x8_identify(port, phone) != 0)
			return -1;
		break;
	case ERICSSON_6XX:
		if (eric_6xx_identify(port, phone) != 0)
			return -1;
		break;
	case ERICSSON_868:
	case ERICSSON_888:
		if (eric_8xx_identify(port, phone) != 0)
			return -1;
		break;
	default:
		fprintf(stderr, "Not supported (yet)\n");
		return -1;
	}

	eric_z80_terminate(port);
	return 0;
}

int action_z80_read_eeprom(struct sp_port *port, struct phone_info *phone)
{
	switch (phone->type) {
	case ERICSSON_6XX:
		if (eric_6xx_read_eeprom(port, phone) != 0)
			return -1;
		break;
	case ERICSSON_868:
	case ERICSSON_888:
		if (eric_8xx_read_eeprom(port, phone) != 0)
			return -1;
		break;
	default:
		fprintf(stderr, "Not supported (yet)\n");
		return -1;
	}

	eric_z80_terminate(port);
	return 0;
}

int action_z80_write_eeprom(struct sp_port *port, struct phone_info *phone, const char *eeprom)
{
	switch (phone->type) {
	case ERICSSON_6XX:
		if (eric_6xx_write_eeprom(port, phone, eeprom) != 0)
			return -1;
		break;
	default:
		fprintf(stderr, "Not supported (yet)\n");
		return -1;
	}

	eric_z80_terminate(port);
	return 0;
}

int action_z80_unlock_usercode(struct sp_port *port, struct phone_info *phone)
{
	switch (phone->type) {
	case ERICSSON_6XX:
		if (eric_6xx_unlock_usercode(port, phone) != 0)
			return -1;
		break;
	default:
		fprintf(stderr, "Not supported (yet)\n");
		return -1;
	}

	eric_z80_terminate(port);
	return 0;
}
