#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <libserialport.h>

#include <core/common.h>
#include <core/connection.h>
#include <core/payload.h>
#include <core/serial.h>
#include <core/io_ctx.h>
#include <emp/common/cmd.h>
#include <emp/common/avr_common.h>
#include <eric/common/eric_common.h>

#include "rtz_arm.h"
#include "rtz_avr.h"
#include "rtz.h"

int action_rtz_identify(struct sp_port *port, struct phone_info *phone)
{
	if (rtz_boot_up(port, phone) != 0)
		return -1;
	if (emp_avr_send_loader(port, "rtz", SETOOL_AVR_RTZ) != 0)
		return -1;
	if (rtz_avr_setool_set_speed(port, phone->baudrate) != 0)
		return -1;
	if (rtz_avr_setool_get_flash_info(port) != 0)
		return -1;

	// Turn-on phone after success every operation
	rtz_setool_turn_on_phone(port);

	return 0;
}

int action_rtz_flash_arm(struct sp_port *port, struct phone_info *phone, const char *fw_name)
{
	if (rtz_boot_up(port, phone) != 0)
		return -1;
	if (emp_avr_send_loader(port, "rtz", SETOOL_AVR_RTZ) != 0)
		return -1;
	if (rtz_avr_setool_set_speed(port, phone->baudrate) != 0)
		return -1;
	if (rtz_arm_init_boot(port) != 0)
		return -1;
	if (rtz_arm_send_bootloader(port, SETOOL_ARM_RTZ) != 0)
		return -1;
	if (rtz_arm_set_com_speed(port, phone->baudrate) != 0)
		return -1;
	if (rtz_arm_get_flash_id(port) != 0)
		return -1;

	if (ends_with(fw_name, ".arm")) {
		if (rtz_arm_flash(port, fw_name) != 0)
			return -1;
	} else {
		printf("Unknown firmware type =)\n");
	}

	// Turn-on phone after success every operation
	rtz_setool_turn_on_phone(port);

	return 0;
}

int action_rtz_flash_avr(struct sp_port *port, struct phone_info *phone, const char *fw_name)
{
	if (ends_with(fw_name, ".bin")) {
		if (rtz_boot_up(port, phone) != 0)
			return -1;
		if (emp_avr_send_loader(port, "rtz", SETOOL_AVR_RTZ) != 0)
			return -1;
		if (rtz_avr_setool_set_speed(port, phone->baudrate) != 0)
			return -1;
		if (rtz_avr_setool_get_flash_info(port) != 0)
			return -1;
		if (rtz_avr_setool_flash_bin(port, 0, fw_name) != 0)
			return -1;
		// Turn-on phone after success every operation
		rtz_setool_turn_on_phone(port);
	} else if (ends_with(fw_name, ".sbn")) {
		if (rtz_boot_up(port, phone) != 0)
			return -1;

		printf("Sending avr-bootloader\n");
		if (emp_avr_send_loader(port, "rtz", ERIC_EMP_RTZ_BOOTSTRAP) != 0)
			return -1;
		if (emp_avr_get_imei(port, phone) != 0)
			return -1;
		if (emp_avr_next_cmd(port) != 0)
			return -1;
		if (emp_avr_set_baudrate(port, 230400) != 0)
			return -1;
		if (emp_avr_next_cmd(port) != 0)
			return -1;

		printf("Sending flash loader\n");
		if (emp_avr_init_cmd(port) != 0)
			return -1;
		if (emp_avr_send_loader(port, "rtz", ERIC_EMP_RTZ_FLASHLOADER) != 0)
			return -1;
		if (emp_avr_set_baudrate(port, phone->baudrate) != 0)
			return -1;
		if (emp_avr_queue_cmd(port) != 0)
			return -1;
		if (emp_avr_get_ldr_v(port) != 0)
			return -1;

		// if (emp_avr_flash_sbn(port, fw_name) != 0)
		// 	return -1;
	} else {
		printf("Unknown firmware type =)\n");
	}

	return 0;
}

int action_rtz_read_avr(struct sp_port *port, struct phone_info *phone, uint32_t addr, uint32_t size)
{
	if (rtz_boot_up(port, phone) != 0)
		return -1;
	if (emp_avr_send_loader(port, "rtz", SETOOL_AVR_RTZ) != 0)
		return -1;
	if (rtz_avr_setool_set_speed(port, phone->baudrate) != 0)
		return -1;
	if (rtz_avr_setool_get_flash_info(port) != 0)
		return -1;
	if (rtz_setool_rip_flash(port, phone->otp_imei, RIP_AVR, addr, size) != 0)
		return -1;

	// Turn-on phone after success every operation
	rtz_setool_turn_on_phone(port);

	return 0;
}

int action_rtz_unlock_usercode(struct sp_port *port, struct phone_info *phone, const char *device_name)
{
	if (rtz_boot_up(port, phone) != 0)
		return -1;

	printf("Sending avr-bootloader\n");
	if (emp_avr_send_loader(port, "rtz", ERIC_EMP_RTZ_BOOTSTRAP) != 0)
		return -1;
	if (emp_avr_get_imei(port, phone) != 0)
		return -1;
	if (emp_avr_set_baudrate(port, 230400) != 0)
		return -1;

	printf("Sending gdfs loader\n");
	if (emp_avr_init_cmd(port) != 0)
		return -1;
	if (emp_avr_send_loader(port, "rtz", emp_avr_get_gdfsloader(device_name)) != 0)
		return -1;
	if (emp_avr_set_baudrate(port, phone->baudrate) != 0)
		return -1;

	send_byte(port, 'Q');
	uint16_t resp;
	recv_half(port, &resp);

	printf("\nReset usercode: ");

	if (avr_f_resetusercode(port) != 0) {
		printf("failed\n");
		return -1;
	}
	printf("done\n");

	emp_avr_finish(port);
	return 0;
}
