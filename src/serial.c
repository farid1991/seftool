#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <libserialport.h>

#include "common.h"
#include "serial.h"

int serial_open(struct sp_port *port)
{
    if (sp_open(port, SP_MODE_READ_WRITE) != SP_OK)
        return -1;
    if (sp_set_baudrate(port, 9600) != SP_OK)
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

int serial_set_baudrate(struct sp_port *port, int baudrate)
{
    struct timespec ts = {0, 1500000}; // 15 ms sleep
    // sleep until phone accepts new baudrate
    nanosleep(&ts, NULL);

    int rc = sp_set_baudrate(port, baudrate);

    // sleep until phone accepts new baudrate
    nanosleep(&ts, NULL);

    return rc;
}

// --- Write helpers ---
int serial_write(struct sp_port *port, const uint8_t *buf, size_t len)
{
    int written = sp_blocking_write(port, buf, len, TIMEOUT);
    if (written < 0)
    {
        fprintf(stderr, "Serial write failed\n");
        return -1;
    }

    // sp_flush(port, SP_BUF_OUTPUT);
    return written;
}

int serial_write_chunks(struct sp_port *port, const uint8_t *buf, size_t len, size_t chunk_size)
{
    for (size_t i = 0; i < len; i += chunk_size)
    {
        int chunk = (i + chunk_size < len) ? chunk_size : (len - i);
        int written = serial_write(port, buf + i, chunk);
        if (written < 0)
            return written;
    }

    // sp_flush(port, SP_BUF_OUTPUT);
    return len;
}

int serial_send_ack(struct sp_port *port)
{
    uint8_t packetdata = SERIAL_ACK;
    if (serial_write(port, &packetdata, 1) < 0)
        return -1;

    return 0;
}

int serial_send_packetdata_ack(struct sp_port *port, const uint8_t *data, size_t len)
{
    uint8_t packetdata = SERIAL_ACK;
    if (serial_write(port, &packetdata, 1) < 0)
        return -1;

    return serial_write(port, data, len);
}

// --- Read helpers ---
int serial_read(struct sp_port *port, uint8_t *buf, size_t bufsize, int timeout_ms)
{
    return sp_blocking_read(port, buf, bufsize, timeout_ms);
}

int serial_wait_packet(struct sp_port *port, uint8_t *buf, size_t bufsize, int timeout_ms)
{
    size_t total = 0;
    int r;

    // Keep looping until timeout or we got something
    while (total < bufsize)
    {
        r = serial_read(port, buf + total, bufsize - total, timeout_ms);
        if (r < 0)
            return r; // error

        if (r == 0)
            break; // timeout, no more data

        total += r;
    }

    return (int)total;
}

int serial_wait_ack(struct sp_port *port, int timeout_ms)
{
    uint8_t resp;
    size_t rcv_len = serial_read(port, &resp, 1, timeout_ms);
    if (rcv_len != 1)
    {
        fprintf(stderr, "\n[serial_wait_ack] Timeout\n");
        return -1;
    }

    if (resp != SERIAL_ACK)
    {
        fprintf(stderr, "\n[serial_wait_ack] Unexpected reply: 0x%02X (expected 0x06)\n", resp);
        return -1;
    }

    return 0;
}

int serial_wait_e3_answer(struct sp_port *port, const char *expected, int timeout_ms, int skiperrors)
{
    uint8_t buf[3];
    size_t received = 0;

    while (received < 3)
    {
        int rcv_len = serial_read(port, buf + received, 3 - received, timeout_ms);
        if (rcv_len <= 0)
        {
            fprintf(stderr, "Read failed (rcv_len=%d)\n", rcv_len);
            return -1;
        }
        received += rcv_len;
    }

    if (memcmp(buf, expected, 3) != 0)
    {
        if (skiperrors == 1)
            return 0;
        fprintf(stderr, "[serial_wait_e3_answer] Unexpected reply: %.*s (expected %s)\n", 3, buf, expected);
        return -1;
    }

    return 0;
}
