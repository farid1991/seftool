#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <libserialport.h>

#include <core/common.h>
#include <core/cli_args.h>
#include <core/connection.h>
#include <emp/v3/flash.h>

#include "action.h"

static void print_usage(const char *progname)
{
	printf("Usage: %s -p <port> -b <baud> -a <action> [options]\n\n", progname);
	printf("  -p, --port <name>                    Serial port name (e.g. COM2, /dev/ttyUSB0)\n");
	printf("  -b, --baud <rate>                    Baudrate (default: 115200)\n");
	printf("  -a, --action <action>                Specify an action (see below)\n\n");

	printf("General actions:\n");
	printf("  %-46s %s\n\n", "identify", "Identify phone and show basic info");

	printf("Platform: Ericsson Z80\n");
	printf("Available actions:\n");
	printf("  %-46s %s\n", "identify", "Identify phone and show basic info");
	printf("  %-46s %s\n", "read-eeprom", "Read full EEPROM");
	printf("  %-46s %s\n", "write-eeprom <eeprom_file>", "Write full EEPROM");
	printf("  %-46s %s\n\n", "unlock usercode", "Reset user code to default");

	printf("Platform: Ericsson AVR\n");
	printf("Available actions:\n");
	printf("  %-46s %s\n", "identify", "Identify phone and show basic info");

	printf("Platform: SE pre-A1 (DB1000)\n");
	printf("Available actions:\n");
	printf("  %-46s %s\n", "identify", "Identify phone and show basic info");
	printf("  %-46s %s\n", "flash-avr <avr_file>", "Flash AVR MCU firmware");
	printf("  %-46s %s\n", "flash-arm <arm_file>", "Flash ARM/DSP firmware");
	printf("  %-46s %s\n\n", "read-avr start <addr> size <bytes>", "Read AVR memory region");
	printf("  %-46s %s\n\n", "unlock-avr <device>",
	       "Unlock userlock (EMP v2 devices: T68,T200,T230,T290,T300,T310,T610,T630,P800,P900,P910,Z600)");

	printf("Platform: SE A1 (DB2000 / DB2010 / DB2012 / DB2020 / PNX5230)\n");
	printf("Available actions:\n");
	printf("  %-46s %s\n", "identify", "Identify phone and show basic info");
	printf("  %-46s %s\n", "flash [main <file>] [fs <file>] [cda <file>]",
	       "Flash MAIN / FS firmware and/or upload CDA");
	printf("  %-46s %s\n", "read-flash start <addr> size <bytes> | block <count>",
	       "Read flash (specify start & size or block count)");
	printf("  %-46s %s\n", "                        save-as-babe", "Optional: convert dump directly to BABE");
	printf("  %-46s %s\n", "read-gdfs", "Backup GDFS area");
	printf("  %-46s %s\n", "write-gdfs <filename>", "Restore GDFS from file");
	printf("  %-46s %s\n", "write-script <file1> [file2 ...]", "Apply VKP or GDFS script(s)");
	printf("  %-46s %s\n", "unlock <usercode|simlock>", "Unlock user code or SIM lock");
	printf("  %-46s %s\n", "fsx-upload src <local...> [dest <fs path>]", "Upload files to internal FS");
	printf("  %-46s %s\n", "fsx-download src <fs path>/<file> [dest <local>]", "Download files from internal FS");
	printf("  %-46s %s\n\n", "upload-anycid", "Upload AnyCID package for exploit access");

	printf("Utility actions:\n");
	printf("  %-46s %s\n", "convert babe2raw <filename>", "Convert BABE -> raw");
	printf("  %-46s %s\n\n", "convert raw2babe <filename> <addr>", "Convert raw -> BABE at address");

	printf("A1 Global options:\n");
	printf("  %-46s %s\n", "--anycid", "Ignore CID restrictions (DB2012/DB2020/PNX5230)");
	printf("  %-46s %s\n\n", "--break-rsa", "Break RSA on DB2000 & DB2010 RED49");

	printf("General options:\n");
	printf("  %-46s %s\n\n", "-h, --help", "Show this help message");
}

int parse_args(int argc, char **argv, cli_args *args)
{
	memset(args, 0, sizeof(*args));
	args->baudrate = 115200;

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0) {
			if (i + 1 < argc)
				args->port_name = argv[++i];
			else
				return fprintf(stderr, "Error: -p requires an argument\n"), 0;
		} else if (strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--baudrate") == 0) {
			if (i + 1 < argc)
				args->baudrate = atoi(argv[++i]);
			else
				return fprintf(stderr, "Error: -b requires an argument\n"), 0;
		} else if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--action") == 0) {
			if (i + 1 < argc)
				args->action = argv[++i];
			else
				return fprintf(stderr, "Error: -a requires an argument\n"), 0;

			/* handle flash extra args */
			if (strcmp(args->action, "flash") == 0) {
				while (i + 1 < argc && argv[i + 1][0] != '-') {
					const char *arg = argv[++i];

					if ((strcmp(arg, "main") == 0 || strcmp(arg, "MAIN") == 0) && i + 1 < argc)
						args->flash_mainfw = argv[++i];
					else if ((strcmp(arg, "fs") == 0 || strcmp(arg, "FS") == 0) && i + 1 < argc)
						args->flash_fsfw = argv[++i];
					else if ((strcmp(arg, "cda") == 0 || strcmp(arg, "CDA") == 0) && i + 1 < argc)
						args->flash_cda = argv[++i];
					else
						return fprintf(stderr,
						               "Error: Unknown or incomplete flash "
						               "argument '%s'\n",
						               arg),
						       0;
				}

				if (!args->flash_mainfw && !args->flash_fsfw && !args->flash_cda)
					return fprintf(stderr, "Error: flash requires at least one "
					                       "of 'main', 'fs', or "
					                       "'cda'\n"),
					       0;
			} else if (strcmp(args->action, "read-flash") == 0) {
				while (i + 1 < argc && argv[i + 1][0] != '-') {
					const char *arg = argv[++i];

					if (strcmp(arg, "start") == 0 && i + 1 < argc)
						args->dump_addr = strtoul(argv[++i], NULL, 0);
					else if (strcmp(arg, "size") == 0 && i + 1 < argc)
						args->dump_size = strtoul(argv[++i], NULL, 0);
					else if (strcmp(arg, "block") == 0 && i + 1 < argc)
						args->dump_size = atoi(argv[++i]) * BLOCK_SIZE;
					else if (strcmp(arg, "save-as-babe") == 0)
						args->save_as_babe = 1;
					else
						return fprintf(stderr, "Error: read-flash requires start "
						                       "<addr> and "
						                       "(size <bytes> | block <count>)\n"),
						       0;
				}

				if (args->dump_addr == 0 || args->dump_size == 0)
					return fprintf(stderr, "Error: read-flash requires start "
					                       "<addr> and (size "
					                       "<bytes> | block <count>)\n"),
					       0;
			} else if (strcmp(args->action, "unlock") == 0) {
				if (i + 1 < argc) {
					args->unlock_target = argv[++i];
					if (strcmp(args->unlock_target, "usercode") != 0 &&
					    strcmp(args->unlock_target, "simlock") != 0)
						return fprintf(stderr, "Error: unlock requires "
						                       "<usercode|simlock>\n"),
						       0;
				} else
					return fprintf(stderr, "Error: unlock requires "
					                       "<usercode|simlock>\n"),
					       0;
			} else if (strcmp(args->action, "write-gdfs") == 0) {
				if (i + 1 < argc)
					args->gdfs_filename = argv[++i];
				else
					return fprintf(stderr, "Error: write-gdfs requires <filename>\n"), 0;
			} else if (strcmp(args->action, "write-script") == 0) {
				int start = i + 1, count = 0;
				while (start + count < argc && argv[start + count][0] != '-')
					count++;
				if (count == 0)
					return fprintf(stderr, "Error: write-script requires at "
					                       "least one <filename>\n"),
					       0;
				args->script_filenames = (const char **)&argv[start];
				args->script_count = count;
				i = start + count - 1;
			} else if (strcmp(args->action, "fsx-upload") == 0) {
				while (i + 1 < argc && argv[i + 1][0] != '-') {
					const char *arg = argv[++i];
					if (strcmp(arg, "src") == 0) {
						int start = i + 1, count = 0;
						while (start + count < argc && argv[start + count][0] != '-' &&
						       strcmp(argv[start + count], "dest") != 0)
							count++;
						if (count == 0)
							return fprintf(stderr, "Error: fsx-upload requires "
							                       "src <file(s)>\n"),
							       0;
						args->src_list = (const char **)&argv[start];
						args->upload_count = count;
						i = start + count - 1;
					} else if (strcmp(arg, "dest") == 0 && i + 1 < argc)
						args->dest_path = argv[++i];
					else
						return fprintf(stderr,
						               "Error: Unknown or incomplete "
						               "fsx-upload argument "
						               "'%s'\n",
						               arg),
						       0;
				}

				if (args->upload_count == 0)
					return fprintf(stderr, "Error: fsx-upload requires src "
					                       "<local path(s)>\n"),
					       0;
			} else if (strcmp(args->action, "fsx-download") == 0) {
				while (i + 1 < argc && argv[i + 1][0] != '-') {
					const char *arg = argv[++i];
					if (strcmp(arg, "src") == 0 && i + 1 < argc)
						args->src_path = argv[++i];
					else if (strcmp(arg, "dest") == 0 && i + 1 < argc)
						args->dest_path = argv[++i];
					else
						return fprintf(stderr,
						               "Error: Unknown or incomplete "
						               "fsx-download argument "
						               "'%s'\n",
						               arg),
						       0;
				}

				if (!args->src_path)
					return fprintf(stderr, "Error: fsx-download requires src "
					                       "<internal path>\n"),
					       0;
				if (!args->dest_path)
					args->dest_path = "./temp";
			} else if (strcmp(args->action, "convert") == 0) {
				if (i + 1 < argc) {
					const char *mode = argv[++i];
					if (strcmp(mode, "raw2babe") == 0) {
						if (i + 2 < argc) {
							args->cnv_mode = mode;
							args->cnv_filename = argv[++i];
							args->mem_addr = strtoul(argv[++i], NULL, 0);
						} else
							return fprintf(stderr, "Error: convert raw2babe "
							                       "requires "
							                       "<filename> <addr>\n"),
							       0;
					} else if (strcmp(mode, "babe2raw") == 0) {
						if (i + 1 < argc)
							args->cnv_mode = mode, args->cnv_filename = argv[++i];
						else
							return fprintf(stderr, "Error: convert babe2raw "
							                       "requires <filename>\n"),
							       0;
					} else
						return fprintf(stderr, "Error: convert requires "
						                       "<raw2babe|babe2raw>\n"),
						       0;
				} else
					return fprintf(stderr, "Error: convert requires "
					                       "<raw2babe|babe2raw> ...\n"),
					       0;
			} else if (strcmp(args->action, "flash-avr") == 0) {
				if (i + 1 < argc)
					args->flash_avr_fw = argv[++i];
				else
					return fprintf(stderr, "Error: flash-avr requires <filename>\n"), 0;
			} else if (strcmp(args->action, "read-avr") == 0) {
				while (i + 1 < argc && argv[i + 1][0] != '-') {
					const char *arg = argv[++i];

					if (strcmp(arg, "start") == 0 && i + 1 < argc)
						args->dump_addr = strtoul(argv[++i], NULL, 0);
					else if (strcmp(arg, "size") == 0 && i + 1 < argc)
						args->dump_size = strtoul(argv[++i], NULL, 0);
					else
						return fprintf(stderr, "Error: read-avr requires start "
						                       "<addr> and size <bytes>\n"),
						       0;
				}

				if (args->dump_addr == 0 || args->dump_size == 0)
					return fprintf(stderr, "Error: read-avr requires start "
					                       "<addr> and (size <bytes> \n"),
					       0;
			} else if (strcmp(args->action, "flash-arm") == 0) {
				if (i + 1 < argc) {
					args->baudrate = 115200; // flash-arm only works with 115200
					args->flash_arm_fw = argv[++i];
				} else
					return fprintf(stderr, "Error: flash-arm requires <filename>\n"), 0;
			} else if (strcmp(args->action, "write-eeprom") == 0) {
				if (i + 1 < argc)
					args->eeprom_file = argv[++i];
				else
					return fprintf(stderr, "Error: write-eeprom requires <filename>\n"), 0;
			} else if (strcmp(args->action, "unlock-avr") == 0) {
				if (i + 1 < argc) {
					args->avr_device_name = argv[++i];
					if (strcmp(args->avr_device_name, "T68") != 0 &&
					    strcmp(args->avr_device_name, "T200") != 0 &&
					    strcmp(args->avr_device_name, "T230") != 0 &&
					    strcmp(args->avr_device_name, "T290") != 0 &&
					    strcmp(args->avr_device_name, "T300") != 0 &&
					    strcmp(args->avr_device_name, "T310") != 0 &&
					    strcmp(args->avr_device_name, "T610") != 0 &&
					    strcmp(args->avr_device_name, "T630") != 0 &&
					    strcmp(args->avr_device_name, "P800") != 0 &&
					    strcmp(args->avr_device_name, "P900") != 0 &&
					    strcmp(args->avr_device_name, "P910") != 0 &&
					    strcmp(args->avr_device_name, "Z600") != 0)
						return fprintf(stderr, "Error: unlock-avr requires "
						                       "<T68|T200|T230|T290|T300|T310|"
						                       "T610|T630|P800|P900|P910|Z600>\n"),
						       0;
				} else
					return fprintf(stderr, "Error: unlock-avr requires "
					                       "<T68|T200|T230|T290|T300|T310|T610|T630|"
					                       "P800|P900|P910|Z600>\n"),
					       0;
			}
		} else if (strcmp(argv[i], "--anycid") == 0)
			args->anycid = 1;
		else if (strcmp(argv[i], "--break-rsa") == 0)
			args->break_rsa = 1;
		else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
			return ACT_NONE;
		else {
			fprintf(stderr, "Unknown option: %s\n", argv[i]);
			return ACT_NONE;
		}
	}

	if (!args->action)
		return ACT_NONE;

	return action_from_string(args->action);
}

int main(int argc, char **argv)
{
	/* parse args */
	cli_args args = {0};
	action_t act = parse_args(argc, argv, &args);

	/* print usage when no action */
	if (act == ACT_NONE) {
		print_usage(argv[0]);
		return 1;
	}

	/* action convert does not need a port */
	if (act == ACT_CONVERT) {
		printf("convert %s\n", args.cnv_mode);
		return action_convert(args.cnv_mode, args.cnv_filename, args.mem_addr);
	}

	/* For all other actions, we need a port */
	if (!args.port_name) {
		print_usage(argv[0]);
		return 1;
	}

	/* create backup for a set of actions */
	switch (act) {
	case ACT_IDENTIFY:
	case ACT_READ_GDFS:
	case ACT_WRITE_GDFS:
	case ACT_READ_FLASH:
	case ACT_READ_EEPROM:
	case ACT_WRITE_EEPROM:
		if (create_backup_dir("backup") != 0)
			return 1;
		break;
	default:
		break;
	}

	/* print parsed args */
	action_print_summary(act, &args);

	/* open port etc */
	struct sp_port *port;
	if (sp_get_port_by_name(args.port_name, &port) != SP_OK) {
		fprintf(stderr, "Error: Cannot open %s\n", args.port_name);
		return 1;
	}

	int res = -1;
	struct phone_info phone = {0};
	phone.baudrate = args.baudrate;

	if (connection_open(port, &phone) != 0)
		goto cleanup;

	// execute pre Ericsson Mobile Protocol
	if (phone.type != ERICSSON_MP) {
		printf("\n");

		if (phone.chipset == PHONE_Z80)
			res = action_phone_z80(port, &phone, act, &args);
		else if (phone.chipset == PHONE_AVR)
			res = action_phone_avr(port, &phone, act, &args);

		goto cleanup;
	}

	switch (phone.protocol_major) {
	case EMP_PROTOCOL_1:
		res = action_emp_v1(port, &phone, act, &args);
		break;
	case EMP_PROTOCOL_2:
		res = action_emp_v2(port, &phone, act, &args);
		break;
	case EMP_PROTOCOL_3:
		res = action_emp_v3(port, &phone, act, &args);
		break;
	default:
		fprintf(stderr, "EMP Protocol %02d.%02d is not supported (yet)", phone.protocol_major,
		        phone.protocol_minor);
		break;
	}

cleanup:
	connection_close(port);
	sp_free_port(port);
	return res;
}
