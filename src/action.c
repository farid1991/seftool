#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <libserialport.h>

#include <core/common.h>
#include <emp/v2/rtz.h>
#include <emp/v3/flash.h>
#include <emp/v3/loader.h>

#include "action.h"

action_t action_from_string(const char *a)
{
	if (!a)
		return ACT_NONE;
	if (strcmp(a, "identify") == 0)
		return ACT_IDENTIFY;
	if (strcmp(a, "flash") == 0)
		return ACT_FLASH;
	if (strcmp(a, "read-flash") == 0)
		return ACT_READ_FLASH;
	if (strcmp(a, "read-gdfs") == 0)
		return ACT_READ_GDFS;
	if (strcmp(a, "write-gdfs") == 0)
		return ACT_WRITE_GDFS;
	if (strcmp(a, "write-script") == 0)
		return ACT_WRITE_SCRIPT;
	if (strcmp(a, "unlock") == 0)
		return ACT_UNLOCK;
	if (strcmp(a, "convert") == 0)
		return ACT_CONVERT;
	if (strcmp(a, "fsx-upload") == 0)
		return ACT_UPLOAD_FS;
	if (strcmp(a, "fsx-download") == 0)
		return ACT_DOWNLOAD_FS;
	if (strcmp(a, "upload-anycid") == 0)
		return ACT_UPLOAD_ANYCID;
	if (strcmp(a, "unlock-avr") == 0)
		return ACT_UNLOCK_AVR;
	if (strcmp(a, "flash-avr") == 0)
		return ACT_FLASH_AVR;
	if (strcmp(a, "read-avr") == 0)
		return ACT_READ_AVR;
	if (strcmp(a, "flash-arm") == 0)
		return ACT_FLASH_ARM;
	if (strcmp(a, "read-arm") == 0)
		return ACT_READ_ARM;
	if (strcmp(a, "flash-z80") == 0)
		return ACT_FLASH_Z80;
	if (strcmp(a, "read-z80") == 0)
		return ACT_READ_Z80;
	if (strcmp(a, "read-eeprom") == 0)
		return ACT_READ_EEPROM;
	if (strcmp(a, "write-eeprom") == 0)
		return ACT_WRITE_EEPROM;
	return ACT_NONE;
}

// Ericsson AVR devices
int action_phone_avr(struct sp_port *port, struct phone_info *phone, action_t act, cli_args *args)
{
	/* execute action */
	switch (act) {
	case ACT_IDENTIFY:
		if (action_avr_identify(port, phone) != 0)
			return -1;
		break;
	case ACT_READ_EEPROM:
		if (action_avr_read_eeprom(port, phone) != 0)
			return -1;
		break;
	case ACT_WRITE_EEPROM:
		if (action_avr_write_eeprom(port, phone, args->eeprom_file) != 0)
			return -1;
		break;
	case ACT_NONE:
	default:
		fprintf(stderr, "Error: unknown or unsupported action '%s'\n", args->action ? args->action : "(null)");
		return -1;
	}
	return 0;
}

// Ericsson Z80 devices
int action_phone_z80(struct sp_port *port, struct phone_info *phone, action_t act, cli_args *args)
{
	/* execute action */
	switch (act) {
	case ACT_IDENTIFY:
		if (action_z80_identify(port, phone) != 0)
			return -1;
		break;

	case ACT_READ_EEPROM:
		if (action_z80_read_eeprom(port, phone) != 0)
			return -1;
		break;

	case ACT_WRITE_EEPROM:
		if (action_z80_write_eeprom(port, phone, args->eeprom_file) != 0)
			return -1;
		break;

	case ACT_UNLOCK:
		if (strcmp(args->unlock_target, "usercode") == 0) {
			if (action_z80_unlock_usercode(port, phone) != 0)
				return -1;
		} else if (strcmp(args->unlock_target, "simlock") == 0) {
			printf("Not implemented (yet)\n");
		}
		break;

	case ACT_NONE:
	default:
		fprintf(stderr, "Error: unknown or unsupported action '%s'\n", args->action ? args->action : "(null)");
		return -1;
	}
	return 0;
}

// Late Ericsson devices
int action_emp_v1(struct sp_port *port, struct phone_info *phone, action_t act, cli_args *args)
{
	/* execute action */
	switch (act) {
	case ACT_IDENTIFY:
		if (emp_avr_action_identify(port, phone) != 0)
			return -1;
		break;

		// case ACT_FLASH_AVR:
		//     if (emp_avr_action_flash_sbn(port, phone, args->flash_avr_fw) != 0)
		//         return -1;
		//     break;

	case ACT_NONE:
	default:
		fprintf(stderr, "Error: unknown or unsupported action '%s'\n", args->action ? args->action : "(null)");
		return -1;
	}
	return 0;
}

// pre A1 devices
int action_emp_v2(struct sp_port *port, struct phone_info *phone, action_t act, cli_args *args)
{
	/* execute action */
	switch (act) {
	case ACT_IDENTIFY:
		if (action_rtz_identify(port, phone) != 0)
			return -1;
		break;

	case ACT_FLASH_ARM:
		if (action_rtz_flash_arm(port, phone, args->flash_arm_fw) != 0)
			return -1;
		break;

	case ACT_FLASH_AVR:
		if (action_rtz_flash_avr(port, phone, args->flash_avr_fw) != 0)
			return -1;
		break;

	case ACT_READ_AVR:
		if (action_rtz_read_avr(port, phone, args->dump_addr, args->dump_size) != 0)
			return -1;
		break;

	case ACT_UNLOCK_AVR:
		if (action_rtz_unlock_usercode(port, phone, args->avr_device_name) != 0)
			return -1;
		break;

	case ACT_NONE:
	default:
		fprintf(stderr, "Error: unknown or unsupported action '%s'\n", args->action ? args->action : "(null)");
		return -1;
	}
	return 0;
}

// A1 devices
int action_emp_v3(struct sp_port *port, struct phone_info *phone, action_t act, cli_args *args)
{
	/* execute action */
	switch (act) {
	case ACT_IDENTIFY:
		if (action_identify(port, phone) != 0)
			return -1;
		break;

	case ACT_UNLOCK:
		if (strcmp(args->unlock_target, "usercode") == 0) {
			phone->gdfs_server = 1;
			if (action_unlock_usercode(port, phone) != 0)
				return -1;
		} else if (strcmp(args->unlock_target, "simlock") == 0) {
			// if (action_unlock_simlock(port, phone) != 0)
			//     return -1;
			printf("Not implemented (yet)\n");
		}
		break;

	case ACT_FLASH:
		phone->break_rsa = args->break_rsa;
		if (phone->break_rsa)
			phone->need_rest = 0;
		if (action_flash_fw(port, phone, args->flash_mainfw, args->flash_fsfw, args->flash_cda) != 0)
			return -1;
		break;

	case ACT_READ_FLASH:
		phone->anycid = args->anycid;
		phone->break_rsa = args->break_rsa;
		phone->need_rest = 1;
		phone->save_as_babe = args->save_as_babe;
		if (action_read_flash(port, phone, args->dump_addr, args->dump_size) != 0)
			return -1;
		break;

	case ACT_READ_GDFS:
		phone->gdfs_server = 1;
		if (action_backup_gdfs(port, phone) != 0)
			return -1;
		break;

	case ACT_WRITE_GDFS:
		phone->gdfs_server = 1;
		if (action_restore_gdfs(port, phone, args->gdfs_filename) != 0)
			return -1;
		break;

	case ACT_WRITE_SCRIPT:
		phone->gdfs_server = 1;
		phone->anycid = args->anycid;
		phone->break_rsa = args->break_rsa;
		phone->need_rest = 1;
		if (action_exec_scripts(port, phone, args->script_count, args->script_filenames) != 0)
			return -1;
		break;

	case ACT_UPLOAD_FS:
		if (action_upload_to_fs(port, phone, args->src_list, args->upload_count,
		                        args->dest_path ? args->dest_path : "/") != 0)
			return -1;
		break;

	case ACT_DOWNLOAD_FS:
		phone->gdfs_server = 1;
		phone->anycid = args->anycid;
		phone->break_rsa = args->break_rsa;
		phone->need_rest = 1;
		if (action_download_from_fs(port, phone, args->src_path, args->dest_path) != 0)
			return -1;
		break;

	case ACT_UPLOAD_ANYCID:
		if (action_upload_anycid(port, phone) != 0)
			return -1;
		break;

	case ACT_NONE:
	default:
		fprintf(stderr, "Error: unknown or unsupported action '%s'\n", args->action ? args->action : "(null)");
		return -1;
	}

	if (loader_shutdown(port) != 0)
		return -1;

	return 0;
}

void action_print_summary(action_t act, const cli_args *args)
{
	printf("Port: %s\n", args->port_name ? args->port_name : "(none)");
	printf("Baudrate: %d\n", args->baudrate);
	printf("Action: %s ", args->action ? args->action : "(none)");

	switch (act) {
	case ACT_FLASH:
		printf("\n");
		if (args->flash_mainfw)
			printf("Main: %s\n", args->flash_mainfw);
		if (args->flash_fsfw)
			printf("FS: %s\n", args->flash_fsfw);
		if (args->flash_cda)
			printf("CDA: %s\n", args->flash_cda);
		break;

	case ACT_READ_FLASH:
		if (args->dump_size % BLOCK_SIZE != 0) {
			uint32_t aligned_size = (args->dump_size + BLOCK_SIZE - 1) & ~(BLOCK_SIZE - 1);
			printf("\nsize 0x%X adjusted to aligned size 0x%X\n", args->dump_size, aligned_size);
		}
		printf("addr: 0x%X, size: 0x%X (%u) bytes\n", args->dump_addr, args->dump_size, args->dump_size);
		if (args->save_as_babe)
			printf("Output saved as BABE format\n");
		break;

	case ACT_UNLOCK:
		printf("%s\n", args->unlock_target);
		break;

	case ACT_WRITE_GDFS:
		printf("%s\n", args->gdfs_filename);
		break;

	case ACT_WRITE_SCRIPT:
		printf("\n");
		for (int i = 0; i < args->script_count; i++)
			printf("[*] %s\n", args->script_filenames[i]);
		printf("\n");
		break;

	case ACT_UPLOAD_FS:
		printf("\nSource(s):\n");
		for (int i = 0; i < args->upload_count; i++)
			printf("[*] %s\n", args->src_list[i]);
		if (args->dest_path)
			printf("Destination: %s\n", args->dest_path);
		else
			printf("Destination: ROOT\n");
		break;

	case ACT_DOWNLOAD_FS:
		printf("\nSource: %s\n", args->src_path);
		printf("Destination: %s\n", args->dest_path);
		break;

	case ACT_FLASH_ARM:
		printf("\n");
		if (args->flash_arm_fw)
			printf("ARM: %s\n", args->flash_arm_fw);
		break;

	case ACT_FLASH_AVR:
		printf("\n");
		if (args->flash_avr_fw)
			printf("AVR: %s\n", args->flash_avr_fw);
		break;

	default:
		printf("\n");
		break;
	}

	printf("\n");
}
