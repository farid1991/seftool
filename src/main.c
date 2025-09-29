#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
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

int connect_phone(struct sp_port *port, int speed)
{
    if (sp_open(port, SP_MODE_READ_WRITE) != SP_OK)
        return -1;
    if (sp_set_baudrate(port, speed) != SP_OK)
        return -1;
    if (sp_set_bits(port, 8) != SP_OK)
        return -1;
    if (sp_set_parity(port, SP_PARITY_NONE) != SP_OK)
        return -1;
    if (sp_set_stopbits(port, 1) != SP_OK)
        return -1;
    if (sp_set_flowcontrol(port, SP_FLOWCONTROL_NONE) != SP_OK)
        return -1;

    sp_set_rts(port, SP_RTS_OFF);
    sp_set_dtr(port, SP_DTR_OFF);
    sp_set_dtr(port, SP_DTR_ON);
    sp_set_rts(port, SP_RTS_ON);

    return SP_OK;
}

int set_speed(struct sp_port *port, struct phone_info *phone, int baudrate)
{
    struct timespec ts = {0, 1000000}; // 10 ms sleep

    // --- DB2000 has max 460800 ---
    if (phone->chip_id == DB2000 && baudrate > 460800)
    {
        printf("DB2000 detected, decrease baudrate.\n");
        baudrate = 460800;
    }

    // --- Fallback if baudrate looks wrong ---
    if (baudrate <= 0)
    {
        printf("Invalid baudrate, falling back to default.\n");
        baudrate = 115200;
    }

    // --- Map baudrate to "Sx" command ---
    const char *speed_char = NULL;
    switch (baudrate)
    {
    case 9600:
        speed_char = "S0";
        break;
    case 19200:
        speed_char = "S1";
        break;
    case 38400:
        speed_char = "S2";
        break;
    case 57600:
        speed_char = "S3";
        break;
    case 115200:
        speed_char = "S4";
        break;
    case 230400:
        speed_char = "S5";
        break;
    case 460800:
        speed_char = "S6";
        break;
    case 921600:
        speed_char = "S7";
        break;
    default:
        speed_char = NULL;
        break;
    }

    if (speed_char)
    {
        serial_write(port, (uint8_t *)speed_char, 2);
    }
    else
    {
        printf("Unknown baudrate %d, using default.\n", baudrate);
        baudrate = 115200;
        serial_write(port, (uint8_t *)"S4", 2); // 115200
    }
    printf("SPEED: %d\n\n", baudrate);

    // sleep until phone accepts new baudrate
    nanosleep(&ts, NULL);

    if (sp_set_baudrate(port, baudrate) != SP_OK)
    {
        fprintf(stderr, "sp_set_baudrate failed\n");
        return -1;
    }

    nanosleep(&ts, NULL);
    return SP_OK;
}

int wait_for_Z(struct sp_port *port)
{
    printf("Powering phone\n");
    printf("Waiting for reply (30s timeout):\n");

    uint8_t c;
    struct timespec start, now;
    clock_gettime(CLOCK_MONOTONIC, &start);

    int last_print = -1; // track last printed second

    while (1)
    {
        int r = sp_blocking_read(port, &c, 1, TIMEOUT);
        if (r > 0)
        {
            if (c == 'Z') // Sony Ericsson reply with 'Z'
            {
                printf("\nConnected\n");
                printf("\nDetected Sony Ericsson\n");
                return SP_OK; // success
            }
        }

        // check elapsed time
        clock_gettime(CLOCK_MONOTONIC, &now);
        double elapsed = (now.tv_sec - start.tv_sec) +
                         (now.tv_nsec - start.tv_nsec) / 1e9;

        int remaining = 30 - (int)elapsed;

        if (remaining != last_print && remaining >= 0)
        {
            printf("\r%2d seconds remaining...", remaining);
            fflush(stdout);
            last_print = remaining;
        }

        if (elapsed > 30.0)
        {
            printf("\nTimeout waiting for phone reply\n");
            return -1; // failed
        }

        // sleep 50ms (enough resolution, not busy spinning)
        struct timespec ts = {0, 50000000};
        nanosleep(&ts, NULL);
    }
}

int send_question_mark(struct sp_port *port, struct phone_info *phone)
{
    uint8_t cmd = '?';

    if (serial_write(port, &cmd, 1) < 0)
        return -1;

    uint8_t resp[8]; // should be 8 bytes according to protocol
    if (sp_blocking_read(port, resp, sizeof(resp), TIMEOUT) <= 0)
        return -1;

    phone->chip_id = ((uint16_t)resp[0] << 8) | resp[1];
    phone->protocol_major = resp[2];
    phone->protocol_minor = (resp[3] == 0xFF) ? 0 : resp[3];
    phone->new_security = (resp[4] == 0x01);

    printf("Chip ID: %04X%s, Platform: %s \n", phone->chip_id,
           phone->new_security ? " [RESPIN]" : "",
           get_chipset_name(phone->chip_id));
    printf("EMP Protocol: %02d.%02d\n", phone->protocol_major, phone->protocol_minor);

    if (phone->protocol_major != 3 || phone->protocol_minor != 1)
    {
        fprintf(stderr, "EMP Protocol %02d.%02d is not supported (yet)", phone->protocol_major, phone->protocol_minor);
        return -1;
    }

    return 0;
}

int erom_get_info(struct sp_port *port, struct phone_info *phone)
{
    if (phone->chip_id == DB2020 || phone->chip_id == 0x5B07 || phone->chip_id == 0x5B08)
        return 0;

    uint8_t resp[128];
    if (phone->chip_id == PNX5230) // --- ICO0 (OTP) ---
    {
        const uint8_t cmd_ico0[] = "ICO0";
        if (serial_write(port, cmd_ico0, sizeof(cmd_ico0) - 1) < 0)
            return -1;

        if (sp_blocking_read(port, resp, sizeof(resp), TIMEOUT) <= 0)
            return -1;

        phone->otp_status = resp[2];
        phone->otp_locked = resp[3];
        phone->otp_cid = (resp[5] << 8) | resp[4];
        phone->otp_paf = resp[6];
        memcpy(phone->otp_imei, resp + 7, 14);
        phone->otp_imei[14] = '\0';
    }
    else // --- IC10 (Certificate) ---
    {
        const uint8_t cmd_ic10[] = "IC10";
        if (serial_write(port, cmd_ic10, sizeof(cmd_ic10) - 1) < 0)
            return -1;

        if (sp_blocking_read(port, resp, sizeof(resp), TIMEOUT) <= 0)
            return -1;

        printf("CERT: %s\n", resp + 2);
    }

    // --- IC30 (Color) ---
    const uint8_t cmd_ic30[] = "IC30";
    if (serial_write(port, cmd_ic30, sizeof(cmd_ic30) - 1) < 0)
        return -1;

    if (sp_blocking_read(port, resp, sizeof(resp), TIMEOUT) <= 0)
        return -1;

    if (resp[2] & 1)
        phone->erom_color = BLUE;
    else if (resp[2] & 2)
        phone->erom_color = BROWN;
    else if (resp[2] & 4)
        phone->erom_color = RED;
    else if (resp[2] & 8)
        phone->erom_color = BLACK;
    else
    {
        fprintf(stderr, "Unknown domain =(\n");
        return -1;
    }

    // --- IC40 (CID) ---
    const uint8_t cmd_ic40[] = "IC40";
    if (serial_write(port, cmd_ic40, sizeof(cmd_ic40) - 1) < 0)
        return -1;

    if (sp_blocking_read(port, resp, sizeof(resp), TIMEOUT) <= 0)
        return -1;

    phone->erom_cid = get_word(&resp[2]);

    printf("PHONE DOMAIN: %s\n", color_get_state(phone->erom_color));
    printf("PHONE CID: %02d\n\n", phone->erom_cid);

    if (phone->chip_id == PNX5230)
    {
        printf("OTP: LOCKED:%d CID:%d PAF:%d IMEI:%s\n",
               phone->otp_locked,
               phone->otp_cid,
               phone->otp_paf,
               phone->otp_imei);
    }

    return 0;
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

    if (connect_phone(port, 9600) != 0)
        goto exit_error;
    if (wait_for_Z(port) != 0)
        goto exit_error;
    if (send_question_mark(port, &phone) != 0)
        goto exit_error;
    if (erom_get_info(port, &phone) != 0)
        goto exit_error;
    if (set_speed(port, &phone, baudrate) != 0)
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
        if (action_flash_fw(port, &phone, flash_mainfw, flash_fsfw) != 0)
            goto exit_error;
        break;

    case ACT_READ_FLASH:
        phone.anycid = anycid;
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
