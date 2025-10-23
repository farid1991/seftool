#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <libserialport.h>

#include "babe.h"
#include "common.h"
#include "connection.h"
#include "cmd.h"
#include "flash.h"
#include "gdfs.h"
#include "loader.h"
#include "serial.h"
#include "action.h"
#include "csloader.h"
#include "rtz.h"
#include "cli_args.h"

// pre A1 devices
int avr_arm_action(struct sp_port *port, struct phone_info *phone, action_t act, cli_args *args)
{
    /* execute action */
    switch (act)
    {
    case ACT_IDENTIFY:
        if (rtz_identify(port, phone) != 0)
            return -1;
        break;

    case ACT_FLASH_ARM:
        if (rtz_flash_arm(port, phone, args->flash_arm_fw) != 0)
            return -1;
        break;

    case ACT_FLASH_AVR:
        if (rtz_flash_avr(port, phone, args->flash_avr_fw) != 0)
            return -1;
        break;

    case ACT_NONE:
    default:
        fprintf(stderr, "Error: unknown action '%s'\n", args->action ? args->action : "(null)");
        return -1;
    }
    return 0;
}

// A1 devices
int new_arm_action(struct sp_port *port, struct phone_info *phone, action_t act, cli_args *args)
{
    /* execute action */
    switch (act)
    {
    case ACT_IDENTIFY:
        if (action_identify(port, phone) != 0)
            return -1;
        break;

    case ACT_UNLOCK:
        if (strcmp(args->unlock_target, "usercode") == 0)
        {
            phone->gdfs_server = 1;
            if (action_unlock_usercode(port, phone) != 0)
                return -1;
        }
        else if (strcmp(args->unlock_target, "simlock") == 0)
        {
            // if (action_unlock_simlock(port, phone) != 0)
            //     return -1;
            printf("Not implemented (yet)\n");
        }
        break;

    case ACT_FLASH:
        phone->break_rsa = args->break_rsa;
        if (action_flash_fw(port, phone, args->flash_mainfw, args->flash_fsfw, args->flash_cda) != 0)
            return -1;
        break;

    case ACT_READ_FLASH:
        phone->anycid = args->anycid;
        phone->break_rsa = args->break_rsa;
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
        if (action_exec_scripts(port, phone, args->script_count, args->script_filenames) != 0)
            return -1;
        break;

    case ACT_UPLOAD_FS:
        if (action_upload_to_fs(port, phone, args->src_list, args->upload_count, args->dest_path ? args->dest_path : "/") != 0)
            return -1;
        break;

    case ACT_DOWNLOAD_FS:
        phone->gdfs_server = 1;
        phone->anycid = args->anycid;
        phone->break_rsa = args->break_rsa;
        if (action_download_from_fs(port, phone, args->src_path, args->dest_path) != 0)
            return -1;
        break;

    case ACT_UPLOAD_ANYCID:
        if (action_upload_anycid(port, phone) != 0)
            return -1;
        break;

    case ACT_NONE:
    default:
        fprintf(stderr, "Error: unknown action '%s'\n", args->action ? args->action : "(null)");
        return -1;
    }

    if (loader_shutdown(port) != 0)
        return -1;

    return 0;
}

void print_action_summary(action_t act, const cli_args *args)
{
    printf("Port: %s\n", args->port_name ? args->port_name : "(none)");
    printf("Baudrate: %d\n", args->baudrate);
    printf("Action: %s ", args->action ? args->action : "(none)");

    switch (act)
    {
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
        if (args->dump_size % BLOCK_SIZE != 0)
        {
            uint32_t aligned_size = (args->dump_size + BLOCK_SIZE - 1) & ~(BLOCK_SIZE - 1);
            printf("\nsize 0x%X adjusted to aligned size 0x%X\n",
                   args->dump_size, aligned_size);
        }
        printf("addr: 0x%X, size: 0x%X (%u) bytes\n",
               args->dump_addr, args->dump_size, args->dump_size);
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

static void print_usage(const char *progname)
{
    printf("Usage: %s -p <port> -b <baud> -a <action> [options]\n\n", progname);
    printf("  -p, --port <name>                       Serial port name (e.g. COM2, /dev/ttyUSB0)\n");
    printf("  -b, --baud <rate>                       Baudrate (default: 115200)\n");
    printf("  -a, --action <action>                   Specify an action (see below)\n\n");

    printf("Platform: pre-A1 (DB1000 — chip IDs 0x5807, 0x5808)\n");
    printf("Available actions:\n");
    printf("  %-36s %s\n", "identify", "Identify phone and show basic info");
    printf("  %-36s %s\n", "flash-avr <avr_file>", "Flash AVR MCU firmware");
    printf("  %-36s %s\n\n", "flash-arm <arm_file>", "Flash ARM/DSP firmware");

    printf("Platform: A1 (DB2000 / DB2010 / DB2012 / DB2020 / PNX5230)\n");
    printf("Available actions:\n");
    printf("  %-36s %s\n", "identify", "Identify phone and show basic info");
    printf("  %-36s %s\n", "flash [main <file>] [fs <file>] [cda <file>]",
           "Flash MAIN / FS firmware and/or upload CDA");
    printf("  %-36s %s\n", "read-flash start <addr> size <bytes> | block <count>",
           "Read flash (specify start & size or block count)");
    printf("  %-36s %s\n", "read-gdfs", "Backup GDFS area");
    printf("  %-36s %s\n", "write-gdfs <filename>", "Restore GDFS from file");
    printf("  %-36s %s\n", "write-script <file1> [file2 ...]", "Apply VKP or GDFS script(s)");
    printf("  %-36s %s\n", "unlock <usercode|simlock>", "Unlock user code or SIM lock");
    printf("  %-36s %s\n", "fsx-upload src <local...> [dest <fs path>]", "Upload files to internal FS");
    printf("  %-36s %s\n", "fsx-download src <fs path>/<file> [dest <local>]", "Download files from internal FS");
    printf("  %-36s %s\n\n", "upload-anycid", "Upload AnyCID package for exploit access");

    printf("Utility actions:\n");
    printf("  %-36s %s\n", "convert babe2raw <filename>", "Convert BABE -> raw");
    printf("  %-36s %s\n\n", "convert raw2babe <filename> <addr>", "Convert raw -> BABE at address");

    printf("A1 Global options:\n");
    printf("  %-36s %s\n", "--anycid", "Ignore CID restrictions (DB2012/DB2020/PNX5230)");
    printf("  %-36s %s\n\n", "--break-rsa", "Break RSA on DB2000 & DB2010 RED49");

    printf("General options:\n");
    printf("  %-36s %s\n\n", "-h, --help", "Show this help message");
}

int parse_args(int argc, char **argv, cli_args *args)
{
    memset(args, 0, sizeof(*args));
    args->baudrate = 115200;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0)
        {
            if (i + 1 < argc)
                args->port_name = argv[++i];
            else
                return fprintf(stderr, "Error: -p requires an argument\n"), 0;
        }
        else if (strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--baudrate") == 0)
        {
            if (i + 1 < argc)
                args->baudrate = atoi(argv[++i]);
            else
                return fprintf(stderr, "Error: -b requires an argument\n"), 0;
        }
        else if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--action") == 0)
        {
            if (i + 1 < argc)
                args->action = argv[++i];
            else
                return fprintf(stderr, "Error: -a requires an argument\n"), 0;

            /* handle flash extra args */
            if (strcmp(args->action, "flash") == 0)
            {
                while (i + 1 < argc && argv[i + 1][0] != '-')
                {
                    const char *arg = argv[++i];

                    if ((strcmp(arg, "main") == 0 || strcmp(arg, "MAIN") == 0) && i + 1 < argc)
                        args->flash_mainfw = argv[++i];
                    else if ((strcmp(arg, "fs") == 0 || strcmp(arg, "FS") == 0) && i + 1 < argc)
                        args->flash_fsfw = argv[++i];
                    else if ((strcmp(arg, "cda") == 0 || strcmp(arg, "CDA") == 0) && i + 1 < argc)
                        args->flash_cda = argv[++i];
                    else
                        return fprintf(stderr, "Error: Unknown or incomplete flash argument '%s'\n", arg), 0;
                }

                if (!args->flash_mainfw && !args->flash_fsfw && !args->flash_cda)
                    return fprintf(stderr, "Error: flash requires at least one of 'main', 'fs', or 'cda'\n"), 0;
            }
            else if (strcmp(args->action, "read-flash") == 0)
            {
                while (i + 1 < argc && argv[i + 1][0] != '-')
                {
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
                        return fprintf(stderr, "Error: read-flash requires start <addr> and (size <bytes> | block <count>)\n"), 0;
                }

                if (args->dump_addr == 0 || args->dump_size == 0)
                    return fprintf(stderr, "Error: read-flash requires start <addr> and (size <bytes> | block <count>)\n"), 0;
            }
            else if (strcmp(args->action, "unlock") == 0)
            {
                if (i + 1 < argc)
                {
                    args->unlock_target = argv[++i];
                    if (strcmp(args->unlock_target, "usercode") != 0 &&
                        strcmp(args->unlock_target, "simlock") != 0)
                        return fprintf(stderr, "Error: unlock requires <usercode|simlock>\n"), 0;
                }
                else
                    return fprintf(stderr, "Error: unlock requires <usercode|simlock>\n"), 0;
            }
            else if (strcmp(args->action, "write-gdfs") == 0)
            {
                if (i + 1 < argc)
                    args->gdfs_filename = argv[++i];
                else
                    return fprintf(stderr, "Error: write-gdfs requires <filename>\n"), 0;
            }
            else if (strcmp(args->action, "write-script") == 0)
            {
                int start = i + 1, count = 0;
                while (start + count < argc && argv[start + count][0] != '-')
                    count++;
                if (count == 0)
                    return fprintf(stderr, "Error: write-script requires at least one <filename>\n"), 0;
                args->script_filenames = (const char **)&argv[start];
                args->script_count = count;
                i = start + count - 1;
            }
            else if (strcmp(args->action, "fsx-upload") == 0)
            {
                while (i + 1 < argc && argv[i + 1][0] != '-')
                {
                    const char *arg = argv[++i];
                    if (strcmp(arg, "src") == 0)
                    {
                        int start = i + 1, count = 0;
                        while (start + count < argc && argv[start + count][0] != '-' &&
                               strcmp(argv[start + count], "dest") != 0)
                            count++;
                        if (count == 0)
                            return fprintf(stderr, "Error: fsx-upload requires src <file(s)>\n"), 0;
                        args->src_list = (const char **)&argv[start];
                        args->upload_count = count;
                        i = start + count - 1;
                    }
                    else if (strcmp(arg, "dest") == 0 && i + 1 < argc)
                        args->dest_path = argv[++i];
                    else
                        return fprintf(stderr, "Error: Unknown or incomplete fsx-upload argument '%s'\n", arg), 0;
                }

                if (args->upload_count == 0)
                    return fprintf(stderr, "Error: fsx-upload requires src <local path(s)>\n"), 0;
            }
            else if (strcmp(args->action, "fsx-download") == 0)
            {
                while (i + 1 < argc && argv[i + 1][0] != '-')
                {
                    const char *arg = argv[++i];
                    if (strcmp(arg, "src") == 0 && i + 1 < argc)
                        args->src_path = argv[++i];
                    else if (strcmp(arg, "dest") == 0 && i + 1 < argc)
                        args->dest_path = argv[++i];
                    else
                        return fprintf(stderr, "Error: Unknown or incomplete fsx-download argument '%s'\n", arg), 0;
                }

                if (!args->src_path)
                    return fprintf(stderr, "Error: fsx-download requires src <internal path>\n"), 0;
                if (!args->dest_path)
                    args->dest_path = "./temp";
            }
            else if (strcmp(args->action, "convert") == 0)
            {
                if (i + 1 < argc)
                {
                    const char *mode = argv[++i];
                    if (strcmp(mode, "raw2babe") == 0)
                    {
                        if (i + 2 < argc)
                        {
                            args->cnv_mode = mode;
                            args->cnv_filename = argv[++i];
                            args->mem_addr = strtoul(argv[++i], NULL, 0);
                        }
                        else
                            return fprintf(stderr, "Error: convert raw2babe requires <filename> <addr>\n"), 0;
                    }
                    else if (strcmp(mode, "babe2raw") == 0)
                    {
                        if (i + 1 < argc)
                            args->cnv_mode = mode, args->cnv_filename = argv[++i];
                        else
                            return fprintf(stderr, "Error: convert babe2raw requires <filename>\n"), 0;
                    }
                    else
                        return fprintf(stderr, "Error: convert requires <raw2babe|babe2raw>\n"), 0;
                }
                else
                    return fprintf(stderr, "Error: convert requires <raw2babe|babe2raw> ...\n"), 0;
            }
            else if (strcmp(args->action, "flash-avr") == 0)
            {
                if (i + 1 < argc)
                    args->flash_avr_fw = argv[++i];
                else
                    return fprintf(stderr, "Error: flash-avr requires <filename>\n"), 0;
            }
            else if (strcmp(args->action, "flash-arm") == 0)
            {
                if (i + 1 < argc)
                {
                    args->baudrate = 115200; // flash-arm only works with 115200
                    args->flash_arm_fw = argv[++i];
                }
                else
                    return fprintf(stderr, "Error: flash-arm requires <filename>\n"), 0;
            }
        }
        else if (strcmp(argv[i], "--anycid") == 0)
            args->anycid = 1;
        else if (strcmp(argv[i], "--break-rsa") == 0)
            args->break_rsa = 1;
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
            return ACT_NONE;
        else
        {
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
    if (act == ACT_NONE)
    {
        print_usage(argv[0]);
        return 1;
    }

    /* action convert does not need a port */
    if (act == ACT_CONVERT)
    {
        printf("convert %s\n", args.cnv_mode);
        return action_convert(args.cnv_mode, args.cnv_filename, args.mem_addr);
    }

    /* For all other actions, we need a port */
    if (!args.port_name)
    {
        print_usage(argv[0]);
        return 1;
    }

    /* create backup for a set of actions */
    switch (act)
    {
    case ACT_IDENTIFY:
    case ACT_READ_GDFS:
    case ACT_WRITE_GDFS:
    case ACT_READ_FLASH:
        if (create_backup_dir("backup") != 0)
            return 1;
        break;
    default:
        break;
    }

    /* print parsed args */
    print_action_summary(act, &args);

    /* open port etc */
    struct sp_port *port;
    if (sp_get_port_by_name(args.port_name, &port) != SP_OK)
    {
        fprintf(stderr, "Error: Cannot open %s\n", args.port_name);
        return 1;
    }

    int res = -1;
    struct phone_info phone = {0};
    phone.baudrate = args.baudrate;

    if (connection_open(port, &phone) != 0)
        goto cleanup;

    switch (phone.protocol_major)
    {
    case EMP_PROTOCOL_2:
        // if (phone.protocol_minor == 1)
        res = avr_arm_action(port, &phone, act, &args);
        break;
    case EMP_PROTOCOL_3:
        // if (phone.protocol_minor == 1)
        res = new_arm_action(port, &phone, act, &args);
        break;
    default:
        fprintf(stderr, "EMP Protocol %02d.%02d is not supported (yet)", phone.protocol_major, phone.protocol_minor);
        break;
    }

cleanup:
    connection_close(port);
    sp_free_port(port);
    return res;
}
