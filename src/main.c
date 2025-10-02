#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <libserialport.h>

#include <sys/stat.h>
#include <errno.h>

#ifdef _WIN32
#include <direct.h> // _mkdir
#define MKDIR(path) _mkdir(path)
#else
#include <unistd.h>
#define MKDIR(path) mkdir(path, 0755)
#endif

#include "babe.h"
#include "common.h"
#include "connection.h"
#include "cmd.h"
#include "flash.h"
#include "gdfs.h"
#include "loader.h"
#include "serial.h"
#include "action.h"

int loader_type = 0;

static int create_backup_dir(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0)
    {
        if (S_ISDIR(st.st_mode))
            return 0; // already exists
        fprintf(stderr, "Error: %s exists but is not a directory\n", path);
        return -1;
    }
    if (MKDIR(path) == 0)
    {
        printf("Created backup directory: %s\n", path);
        return 0;
    }
    if (errno == EEXIST) // race condition safety
        return 0;

    perror("mkdir");
    return -1;
}

static void print_usage(const char *progname)
{
    printf("Usage: %s -p <port> -b <baud> -a <action> [options]\n\n", progname);
    printf("  -p, --port <name>       Serial port name (e.g. COM2, /dev/ttyUSB0)\n");
    printf("  -b, --baud <rate>       Baudrate (default: 115200)\n");
    printf("  -a, --action <action>   Action:\n");
    printf("                          identify\n");
    printf("                          flash <main> xor <fs>\n");
    printf("                          read-flash start <addr> size <bytes> OR block <count>\n");
    printf("                            [save-as-babe]\n");
    printf("                          read-gdfs\n");
    printf("                          write-gdfs <filename>\n");
    printf("                          write-script <file1> [file2 ...]\n");
    printf("                          unlock <usercode|simlock>\n");
    printf("                          convert babe2raw <filename>\n");
    printf("                          convert raw2babe <filename> <addr>\n");
    printf("\nGlobal options:\n");
    printf("    --anycid              Ignore CID restrictions (DB2012/DB2020/PNX5230)\n");
    printf("    --break-rsa           Break RSA on DB2000 & DB2010 RED49\n");
    printf("  -h, --help              Show this help message\n");
}

int main(int argc, char **argv)
{
    const char *port_name = NULL;
    int baudrate = 115200; // default
    const char *action = NULL;
    const char *unlock_target = NULL;
    const char *gdfs_filename = NULL;
    const char *flash_mainfw = NULL;
    const char *flash_fsfw = NULL;
    const char *cnv_filename = NULL;
    const char *cnv_mode = NULL;
    const char **script_filenames = NULL;
    int script_count = 0;

    uint32_t dump_addr = 0;
    uint32_t dump_size = 0;
    uint32_t mem_addr = 0;

    int anycid = 0;
    int break_rsa = 0;
    int save_as_babe = 0;

    /* parse args */
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0)
        {
            if (i + 1 < argc)
                port_name = argv[++i];
            else
            {
                fprintf(stderr, "Error: -p requires an argument\n");
                return 1;
            }
        }
        else if (strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--baudrate") == 0)
        {
            if (i + 1 < argc)
                baudrate = atoi(argv[++i]);
            else
            {
                fprintf(stderr, "Error: -b requires an argument\n");
                return 1;
            }
        }
        else if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--action") == 0)
        {
            if (i + 1 < argc)
                action = argv[++i];
            else
            {
                fprintf(stderr, "Error: -a requires an argument\n");
                return 1;
            }

            // handle flash extra args right after '-a flash'
            if (strcmp(action, "flash") == 0)
            {
                if (i + 1 < argc)
                    flash_mainfw = argv[++i]; // first filename (required)
                else
                {
                    fprintf(stderr, "Error: flash requires at least one <filename>\n");
                    return 1;
                }

                if (i + 1 < argc && argv[i + 1][0] != '-')
                {
                    flash_fsfw = argv[++i]; // optional second filename
                }
            }
            // handle read-flash extra args right after '-a read-flash'
            else if (strcmp(action, "read-flash") == 0)
            {
                // expect keywords: start <addr> size <bytes> OR block <count>
                while (i + 1 < argc && argv[i + 1][0] != '-')
                {
                    const char *arg = argv[++i];

                    if (strcmp(arg, "start") == 0 && i + 1 < argc)
                    {
                        dump_addr = strtoul(argv[++i], NULL, 0);
                    }
                    else if (strcmp(arg, "size") == 0 && i + 1 < argc)
                    {
                        dump_size = strtoul(argv[++i], NULL, 0);
                    }
                    else if (strcmp(arg, "block") == 0 && i + 1 < argc)
                    {
                        int blocks = atoi(argv[++i]);
                        dump_size = blocks * BLOCK_SIZE;
                    }
                    else if (strcmp(arg, "save-as-babe") == 0)
                    {
                        save_as_babe = 1;
                    }
                    else
                    {
                        fprintf(stderr, "Error: read-flash requires start <addr> and (size <bytes> | block <count>)\n");
                        return 1;
                    }
                }

                if (dump_addr == 0 || dump_size == 0)
                {
                    fprintf(stderr, "Error: read-flash requires start <addr> and (size <bytes> | block <count>)\n");
                    return 1;
                }
            }
            else if (strcmp(action, "unlock") == 0)
            {
                if (i + 1 < argc)
                {
                    unlock_target = argv[++i];
                    if (strcmp(unlock_target, "usercode") != 0 &&
                        strcmp(unlock_target, "simlock") != 0)
                    {
                        fprintf(stderr, "Error: unlock requires <usercode|simlock>\n");
                        return 1;
                    }
                }
                else
                {
                    fprintf(stderr, "Error: unlock requires <usercode|simlock>\n");
                    return 1;
                }
            }
            else if (strcmp(action, "write-gdfs") == 0)
            {
                if (i + 1 < argc)
                {
                    gdfs_filename = argv[++i];
                }
                else
                {
                    fprintf(stderr, "Error: write-gdfs requires <filename>\n");
                    return 1;
                }
            }
            else if (strcmp(action, "write-script") == 0)
            {
                // Collect all remaining args until a '-' or end
                int start = i + 1;
                int count = 0;
                while (start + count < argc && argv[start + count][0] != '-')
                {
                    count++;
                }
                if (count == 0)
                {
                    fprintf(stderr, "Error: write-script requires at least one <filename>\n");
                    return 1;
                }
                script_filenames = (const char **)&argv[start]; // pointer into argv
                script_count = count;
                i = start + count - 1; // move index
            }
            else if (strcmp(action, "convert") == 0)
            {
                if (i + 1 < argc)
                {
                    const char *mode = argv[++i];

                    if (strcmp(mode, "raw2babe") == 0)
                    {
                        if (i + 2 < argc)
                        {
                            cnv_mode = mode;
                            cnv_filename = argv[++i];
                            mem_addr = strtoul(argv[++i], NULL, 0);
                        }
                        else
                        {
                            fprintf(stderr, "Error: convert raw2babe requires <filename> <addr>\n");
                            return 1;
                        }
                    }
                    else if (strcmp(mode, "babe2raw") == 0)
                    {
                        if (i + 1 < argc)
                        {
                            cnv_mode = mode;
                            cnv_filename = argv[++i];
                        }
                        else
                        {
                            fprintf(stderr, "Error: convert babe2raw requires <filename>\n");
                            return 1;
                        }
                    }
                    else
                    {
                        fprintf(stderr, "Error: convert requires <raw2babe|babe2raw>\n");
                        return 1;
                    }
                }
                else
                {
                    fprintf(stderr, "Error: convert requires <raw2babe|babe2raw> ...\n");
                    return 1;
                }
            }
        }
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
        {
            print_usage(argv[0]);
            return 0;
        }
        else if (strcmp(argv[i], "--anycid") == 0)
        {
            anycid = 1;
        }
        else if (strcmp(argv[i], "--break-rsa") == 0)
        {
            break_rsa = 1;
        }
        else
        {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    if (!action)
    {
        print_usage(argv[0]);
        return 1;
    }

    action_t act = action_from_string(action);

    if (act == ACT_NONE)
    {
        print_usage(argv[0]);
        return 1;
    }

    /* convert does not need a port */
    if (act == ACT_CONVERT)
    {
        printf("convert %s\n", cnv_mode);
        return action_convert(cnv_mode, cnv_filename, mem_addr);
    }

    /* For all other actions, we need a port */
    if (!port_name)
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
    printf("Port: %s\n", port_name);
    printf("Baudrate: %d\n", baudrate);
    printf("Action: %s ", action);

    switch (act)
    {
    case ACT_READ_FLASH:
        if (dump_size % BLOCK_SIZE != 0)
        {
            uint32_t aligned_size = (dump_size + BLOCK_SIZE - 1) & ~(BLOCK_SIZE - 1);
            printf("\nsize 0x%X adjusted to aligned size 0x%X\n", dump_size, aligned_size);
            dump_size = aligned_size;
        }
        printf("addr: 0x%X, size: 0x%X (%u) bytes\n", dump_addr, dump_size, dump_size);
        if (save_as_babe)
            printf("Output saved as BABE format\n");
        break;
    case ACT_UNLOCK:
        printf("%s\n", unlock_target);
        break;
    case ACT_WRITE_GDFS:
        printf("%s\n", gdfs_filename);
        break;
    case ACT_WRITE_SCRIPT:
        for (int i = 0; i < script_count; i++)
            printf("%s ", script_filenames[i]);
        printf("\n");
        break;
    default:
        printf("\n");
        break;
    }

    printf("\n");

    /* open port etc */
    struct sp_port *port;
    if (sp_get_port_by_name(port_name, &port) != SP_OK)
    {
        fprintf(stderr, "Error: Cannot open %s\n", port_name);
        return 1;
    }

    struct phone_info phone = {0};
    phone.baudrate = baudrate;
    if (connection_open(port, &phone) != 0)
        goto exit_error;

    /* execute action */
    switch (act)
    {
    case ACT_IDENTIFY:
        if (action_identify(port, &phone) != 0)
            goto exit_error;
        break;

    case ACT_UNLOCK:
        if (strcmp(unlock_target, "usercode") == 0)
        {
            if (action_unlock_usercode(port, &phone) != 0)
                goto exit_error;
        }
        else if (strcmp(unlock_target, "simlock") == 0)
        {
            // if (action_unlock_simlock(port, &phone) != 0)
            //     goto exit_error;
            printf("Not implemented (yet)\n");
        }
        break;

    case ACT_FLASH:
        phone.break_rsa = break_rsa;
        if (action_flash_fw(port, &phone, flash_mainfw, flash_fsfw) != 0)
            goto exit_error;
        break;

    case ACT_READ_FLASH:
        phone.anycid = anycid;
        phone.break_rsa = break_rsa;
        phone.save_as_babe = save_as_babe;
        if (action_read_flash(port, &phone, dump_addr, dump_size) != 0)
            goto exit_error;
        break;

    case ACT_READ_GDFS:
        if (action_backup_gdfs(port, &phone) != 0)
            goto exit_error;
        break;

    case ACT_WRITE_GDFS:
        if (action_restore_gdfs(port, &phone, gdfs_filename) != 0)
            goto exit_error;
        break;

    case ACT_WRITE_SCRIPT:
        phone.anycid = anycid;
        phone.break_rsa = break_rsa;
        if (action_exec_scripts(port, &phone, script_count, script_filenames) != 0)
            goto exit_error;
        break;

    case ACT_NONE:
    default:
        fprintf(stderr, "Error: unknown action '%s'\n", action ? action : "(null)");
        goto exit_error;
    }

    if (loader_shutdown(port) != 0)
        goto exit_error;

    sp_close(port);
    sp_free_port(port);
    return 0;

exit_error:
    sp_close(port);
    sp_free_port(port);
    return -1;
}
