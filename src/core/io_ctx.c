#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <libserialport.h>

#include "common.h"
#include "io_ctx.h"

// --- 8-bit (Byte) ---
int recv_byte(struct sp_port *port)
{
	uint8_t b = 0;
	int r = serial_read(port, &b, 1, 20 * TIMEOUT);
	return (r == 1) ? b : -1;
}

void send_byte(struct sp_port *port, uint8_t b) { serial_write(port, &b, 1); }

// --- 16-bit (Half) ---
void send_half(struct sp_port *port, uint16_t value)
{
	send_byte(port, value >> 8);
	send_byte(port, value);
}

int recv_half(struct sp_port *port, uint16_t *value)
{
	uint8_t buf[2];
	int r = serial_read(port, buf, 2, 20 * TIMEOUT);
	if (r != 2)
		return -1;
	*value = ((uint16_t)buf[0] << 8) | buf[1];
	return 0;
}

// --- 24-bit (mediumword) ---
void send_mediumword(struct sp_port *port, uint32_t value)
{
	send_byte(port, value >> 16);
	send_byte(port, value >> 8);
	send_byte(port, value);
}

int recv_mediumword(struct sp_port *port, uint32_t *value)
{
	uint8_t buf[3];
	int r = serial_read(port, buf, 3, 20 * TIMEOUT);
	if (r != 3)
		return -1;
	*value = ((uint32_t)buf[0] << 16) | ((uint32_t)buf[1] << 8) | buf[2];
	return 0;
}

// --- 32-bit (word) ---
void send_word(struct sp_port *port, uint32_t value)
{
	send_byte(port, value >> 24);
	send_byte(port, value >> 16);
	send_byte(port, value >> 8);
	send_byte(port, value);
}

int recv_word(struct sp_port *port, uint32_t *value)
{
	uint8_t buf[4];
	int r = serial_read(port, buf, 4, 20 * TIMEOUT);
	if (r != 4)
		return -1;
	*value = ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) | ((uint32_t)buf[2] << 8) | buf[3];
	return 0;
}

// --- Block (array) ---
void send_block(struct sp_port *port, const void *src, size_t len) { serial_write(port, src, len); }

void send_chunk(struct sp_port *port, const void *src, size_t len) { serial_write_chunks(port, src, len, 0x100); }

int recv_block(struct sp_port *port, void *dst, size_t len) { return serial_read(port, dst, len, 20 * TIMEOUT); }

int wait_for_byte(struct sp_port *port, uint8_t wb)
{
	const int max_wait = 50 * TIMEOUT;
	int elapsed = 0;
	int b;

	while (elapsed < max_wait) {
		b = recv_byte(port);
		if (b == wb)
			return 0; // success
		if (b == -1)
			return -1; // serial error
		elapsed += TIMEOUT;
	}
	return 1; // timeout
}

int wait_for_answer(struct sp_port *port, const char *expected, size_t len, int skiperrors)
{
	uint8_t resp[64] = {0};
	int rcv_len = recv_block(port, resp, sizeof(resp));
	if (rcv_len <= 0)
		return -1;

	resp[rcv_len] = '\0';

	// Expected response
	if (memcmp(resp, expected, len) == 0)
		return 0;

	if (skiperrors)
		return 0;

	return -1;
}

void flush(struct sp_port *port) { sp_flush(port, SP_BUF_BOTH); }
