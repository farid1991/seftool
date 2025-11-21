#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <libserialport.h>

#include <core/common.h>
#include <core/connection.h>
#include <core/serial.h>
#include <core/io_ctx.h>
#include <core/payload.h>

#include <eric/common/eric_common.h>
#include "eric_z80.h"

#define EEPROM_SIZE 0x2000
#define EEPROM_BASE EEPROM_SIZE

int eric_6xx_init_serv_mode(struct sp_port *port, struct phone_info *phone)
{
	if (eric_init_cmd(port) != 0)
		return -1;
	if (eric_z80_send_loader(port, ERIC_6XX_BOOTSTRAP) != 0)
		return -1;
	if (eric_z80_get_ldr_version(port, phone) != 0)
		return -1;
	if (eric_z80_set_speed(port, phone->baudrate) != 0)
		return -1;
	if (eric_z80_send_byte(port, '\r') != 0)
		return -1;

	if (eric_init_cmd(port) != 0)
		return -1;
	if (eric_z80_send_loader(port, ERIC_6XX_BOOTLOADER) != 0)
		return -1;
	if (eric_z80_set_speed(port, 9600) != 0)
		return -1;
	if (eric_z80_send_byte(port, '\r') != 0)
		return -1;

	send_byte(port, 'Q');
	if (wait_for_answer(port, "SP,OK\r\r\n", 8, 0) != 0)
		return -1;

	if (eric_z80_set_baudrate(port, phone->baudrate) != 0)
		return -1;

	if (eric_z80_get_vers(port, phone, LOADER_VERS) != 0)
		return -1;
	printf("LDR: %s\n", phone->ldr_vers);

	printf("Phone in Service Mode\n\n");

	if (eric_z80_get_eep_imei(port, phone) != 0)
		return -1;
	printf("IMEI: %s\n", phone->eep_imei);

	return 0;
}

int eric_6xx_get_channel_ind(struct sp_port *port)
{
	printf("CHANNEL IND: ");

	uint16_t ChannelIndicationAllowed = 0x03EE;

	uint8_t val;
	if (eric_read_eeprom_byte(port, EEPROM_BASE, ChannelIndicationAllowed, &val) == 0)
		printf("%s\n", val ? "ON" : "OFF");
	else
		printf("Read failed\n");

	return 0;
}

int eric_6xx_get_sm_capability(struct sp_port *port)
{
	printf("SM Capability: ");

	uint16_t GD_SM_Capability = 0x1866;

	uint8_t val;
	if (eric_read_eeprom_byte(port, EEPROM_BASE, GD_SM_Capability, &val) == 0)
		printf("%s\n", val ? "ON" : "OFF");
	else
		printf("Read failed\n");

	return 0;
}

void eric_6xx_read_splock(struct sp_port *port, struct phone_info *phone)
{
	eric_decode_splock(port, EEPROM_BASE, 0x0810, phone->nck);
	eric_decode_splock(port, EEPROM_BASE, 0x0818, phone->nsck);
	eric_decode_splock(port, EEPROM_BASE, 0x0820, phone->cck);
}

void eric_6xx_read_phonelock(struct sp_port *port, struct phone_info *phone)
{
	eric_decode_phonelock(port, EEPROM_BASE, 0x03CF, phone->elock);
}

int eric_6xx_identify(struct sp_port *port, struct phone_info *phone)
{
	if (eric_6xx_init_serv_mode(port, phone) != 0)
		return -1;

	if (eric_z80_get_vers(port, phone, FIRMWARE_VERS) != 0)
		return -1;
	printf("FIRMWARE: %s\n", phone->fw_vers);

	eric_6xx_get_channel_ind(port);

	eric_6xx_read_phonelock(port, phone);
	printf("ELCK : %s\n", phone->elock);

	eric_6xx_read_splock(port, phone);
	printf("NCK : %s\n", phone->nck);
	printf("NSCK: %s\n", phone->nsck);
	printf("CCK : %s\n", phone->cck);

	// eric_get_imei_data(port, phone, EEPROM_BASE, 0x0582);
	// eric_change_imei(port, EEPROM_BASE, 0x0582);

	return 0;
}

int eric_6xx_read_eeprom(struct sp_port *port, struct phone_info *phone)
{
	if (eric_6xx_init_serv_mode(port, phone) != 0)
		return -1;

	char dump_name[256];
	snprintf(dump_name, sizeof(dump_name), "./backup/eeprom_dump_%s.bin", phone->eep_imei);

	return eric_read_eeprom(port, dump_name, EEPROM_BASE, EEPROM_SIZE);
}

int eric_6xx_write_eeprom(struct sp_port *port, struct phone_info *phone, const char *eeprom)
{
	if (eric_6xx_init_serv_mode(port, phone) != 0)
		return -1;

	size_t fsize;
	uint8_t *data = load_file(eeprom, &fsize);
	if (!data) {
		fprintf(stderr, "Error: cannot load EEPROM image %s\n", eeprom);
		return -1;
	}
	free(data);

	if (fsize != EEPROM_SIZE) {
		fprintf(stderr, "Error: invalid EEPROM size %zu\n", fsize);
		return -1;
	}

	return eric_write_eeprom(port, eeprom, EEPROM_BASE);
}

int eric_6xx_unlock_usercode(struct sp_port *port, struct phone_info *phone)
{
	if (eric_6xx_init_serv_mode(port, phone) != 0)
		return -1;

	uint16_t GD_MMI_ElectronicLock = 0x03CF;
	for (int i = 0; i < 12; i++) {
		eric_write_eeprom_byte(port, EEPROM_BASE, GD_MMI_ElectronicLock + i, 0);
	}

	return 0;
}
