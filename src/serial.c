#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <libserialport.h>

#include "common.h"
#include "serial.h"

// --- Write helpers ---
int serial_write(struct sp_port *port, const uint8_t *buf, int len)
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

int serial_write_chunks(struct sp_port *port, const uint8_t *buf, int len, int chunk_size)
{
    for (int i = 0; i < len; i += chunk_size)
    {
        int chunk = (i + chunk_size < len) ? chunk_size : (len - i);
        int written = sp_blocking_write(port, buf + i, chunk, TIMEOUT);
        if (written < 0)
            return written;
    }

    // sp_flush(port, SP_BUF_OUTPUT);
    return len;
}

int serial_wait_ack(struct sp_port *port, int timeout_ms)
{
    uint8_t resp;
    int rlen = serial_wait_packet(port, &resp, 1, timeout_ms);
    if (rlen != 1)
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

int serial_wait_packet(struct sp_port *port, uint8_t *buf, size_t bufsize, int timeout_ms)
{
    size_t total = 0;
    int r;

    // Keep looping until timeout or we got something
    while (total < bufsize)
    {
        r = sp_blocking_read(port, buf + total, bufsize - total, timeout_ms);
        if (r < 0)
            return r; // error

        if (r == 0)
            break; // timeout, no more data

        total += r;
    }

    return (int)total;
}
