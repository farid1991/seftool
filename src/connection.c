#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <libserialport.h>

#include "babe.h"
#include "common.h"
#include "serial.h"

int set_speed(struct sp_port *port, struct phone_info *phone)
{
    // --- DB2000 has max 460800 ---
    if (phone->chip_id == DB2000 && phone->baudrate > 460800)
    {
        printf("DB2000 detected, decrease baudrate.\n");
        phone->baudrate = 460800;
    }

    // --- Fallback if baudrate looks wrong ---
    if (phone->baudrate <= 0)
    {
        printf("Invalid baudrate, falling back to default.\n");
        phone->baudrate = 115200;
    }

    // --- Map baudrate to "Sx" command ---
    const char *speed_char = get_speed_chars(phone->baudrate);
    if (speed_char)
    {
        serial_write(port, (uint8_t *)speed_char, 2);
    }
    else
    {
        printf("Unknown baudrate %d, using default.\n", phone->baudrate);
        phone->baudrate = 115200;
        serial_write(port, (uint8_t *)"S4", 2); // fallback to 115200
    }

    printf("SPEED: %d\n\n", phone->baudrate);

    if (serial_set_baudrate(port, phone->baudrate) != SP_OK)
    {
        fprintf(stderr, "sp_set_baudrate failed\n");
        return -1;
    }

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
        int r = serial_read(port, &c, 1, TIMEOUT);
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
    if (serial_read(port, resp, sizeof(resp), TIMEOUT) <= 0)
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

        if (serial_read(port, resp, sizeof(resp), TIMEOUT) <= 0)
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

        if (serial_read(port, resp, sizeof(resp), TIMEOUT) <= 0)
            return -1;

        printf("CERT: %s\n", resp + 2);
    }

    // --- IC30 (Color) ---
    const uint8_t cmd_ic30[] = "IC30";
    if (serial_write(port, cmd_ic30, sizeof(cmd_ic30) - 1) < 0)
        return -1;

    if (serial_read(port, resp, sizeof(resp), TIMEOUT) <= 0)
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

    if (serial_read(port, resp, sizeof(resp), TIMEOUT) <= 0)
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

int connection_open(struct sp_port *port, struct phone_info *phone)
{
    if (serial_open(port) != 0)
        return -1;
    if (wait_for_Z(port) != 0)
        return -1;
    if (send_question_mark(port, phone) != 0)
        return -1;
    if (erom_get_info(port, phone) != 0)
        return -1;
    if (set_speed(port, phone) != 0)
        return -1;

    return 0;
}

int connection_close(struct sp_port *port)
{
    return sp_close(port);
}