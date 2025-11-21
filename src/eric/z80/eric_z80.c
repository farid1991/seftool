#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <libserialport.h>

#include <core/common.h>
#include <core/connection.h>
#include <core/serial.h>
#include <core/io_ctx.h>

#include <eric/common/eric_common.h>
#include "eric_z80.h"

int eric_z80_set_speed(struct sp_port *port, int baudrate)
{
	if (baudrate > 115200)
		baudrate = 115200;

	const char *cmd = eric_get_speed_val(baudrate);
	send_block(port, cmd, 3);

	// sleep until phone accepts new baudrate
	sleep_ms(200);

	if (serial_set_baudrate(port, baudrate) != 0)
		return -1;

	// sleep until phone accepts new baudrate
	sleep_ms(500);
	return 0;
}

int eric_z80_send_byte(struct sp_port *port, uint8_t cmd)
{
	send_byte(port, cmd);

	return wait_for_byte(port, '>');
}

int eric_z80_send_loader(struct sp_port *port, const char *fname)
{
	printf("Sending loader... ");

	char loader_name[512];
	snprintf(loader_name, sizeof(loader_name), "./loader/z80/%s", fname);

	size_t ldr_size;
	uint8_t *ldr = load_file(loader_name, &ldr_size);

	// Send data block
	serial_write_chunks(port, ldr, ldr_size, 0x100);

	free(ldr);

	if (wait_for_byte(port, '>') != 0)
		return -1;

	printf("OK\n");

	return 0;
}

int eric_z80_get_imei(struct sp_port *port, struct phone_info *phone)
{
	send_byte(port, 'I');
	uint8_t resp[24] = {0};
	int rcv_len = recv_block(port, resp, sizeof(resp));
	if (rcv_len <= 0)
		return -1;

	memcpy(phone->eep_imei, resp, rcv_len);
	phone->eep_imei[rcv_len] = '\0';

	printf("IMEI: %s\n", phone->eep_imei);

	return 0;
}

int eric_z80_get_ldr_version(struct sp_port *port, struct phone_info *phone)
{
	send_byte(port, 'V');

	uint8_t resp[40] = {0};
	int rcv_len = recv_block(port, resp, sizeof(resp));
	if (rcv_len <= 0)
		return -1;

	resp[rcv_len] = '\0';

	// Convert to string for parsing
	char *str = (char *)resp;

	// Skip leading "\r\n>"
	char *start = strstr(str, ">");
	if (start)
		start++; // move past '>'

	// Find trailing "\r\n>"
	char *end = strstr(start, "\r\n>");
	if (end)
		*end = '\0'; // terminate before it

	// Copy to phone->ldr_vers
	strncpy(phone->ldr_vers, start, sizeof(phone->ldr_vers) - 1);
	phone->ldr_vers[sizeof(phone->ldr_vers) - 1] = '\0';

	printf("LDR: %s\n", phone->ldr_vers);

	return 0;
}

int eric_z80_get_vers(struct sp_port *port, struct phone_info *phone, int version)
{
	char vers[8];
	snprintf(vers, sizeof(vers), "VERS %d\r", version);
	send_block(port, vers, sizeof(vers) - 1);

	uint8_t resp[40] = {0};
	int rcv_len = recv_block(port, resp, sizeof(resp));
	if (rcv_len <= 0)
		return -1;

	resp[rcv_len] = '\0';

	if (version == LOADER_VERS) {
		if (eric_parse_response((char *)resp, "VERS", phone->ldr_vers, sizeof(phone->ldr_vers)) != 0)
			return -1;
	} else {
		if (eric_parse_response((char *)resp, "VERS", phone->fw_vers, sizeof(phone->fw_vers)) != 0)
			return -1;
	}

	return 0;
}

int eric_z80_get_eep_imei(struct sp_port *port, struct phone_info *phone)
{
	send_block(port, "LIME\r", 5);

	uint8_t resp[30] = {0};
	int rcv_len = recv_block(port, resp, sizeof(resp));
	if (rcv_len <= 0)
		return -1;

	resp[rcv_len] = '\0';

	if (eric_parse_response((char *)resp, "LIME", phone->eep_imei, sizeof(phone->eep_imei)) != 0)
		return -1;

	return 0;
}

int eric_z80_get_baudrate(struct sp_port *port)
{
	send_block(port, "BAUD\r", 5);

	uint8_t resp[32] = {0};
	int rcv_len = recv_block(port, resp, sizeof(resp));
	if (rcv_len <= 0)
		return -1;

	resp[rcv_len] = '\0';
	char speed[9] = {0};
	if (eric_parse_response((char *)resp, "BAUD", speed, sizeof(speed)) != 0)
		return -1;

	printf("SPEED: %s\n", speed);

	return 0;
}

int eric_z80_set_baudrate(struct sp_port *port, int baudrate)
{
	int baud_code;

	// Map actual baudrate to Ericsson protocol code (x = 1..4)
	switch (baudrate) {
	case 9600:
		baud_code = 1;
		break;
	case 19200:
		baud_code = 2;
		break;
	case 38400:
		baud_code = 3;
		break;
	case 115200:
	default:
		baud_code = 4;
		baudrate = 115200;
		break;
	}

	char cmd[16];
	snprintf(cmd, sizeof(cmd), "BAUD %d\r", baud_code);

	send_block(port, cmd, strlen(cmd));

	sleep_ms(1000);

	if (serial_set_baudrate(port, baudrate) != 0) {
		fprintf(stderr, "Failed to set local baudrate\n");
		return -1;
	}

	sleep_ms(1000);

	send_byte(port, '\r');

	// Confirm new speed with "BAUD,OK" response
	if (wait_for_answer(port, "BAUD,OK\r\r\n", 10, 0) != 0) {
		fprintf(stderr, "No response after baudrate change\n");
		return -1;
	}

	return 0;
}

static void cmd_copy_result(char *dest, size_t dest_size, const char *src, const char *end)
{
	size_t len = end - src;
	if (len >= dest_size)
		len = dest_size - 1;
	memcpy(dest, src, len);
	dest[len] = '\0';
}

int eric_parse_response(const char *resp, const char *cmd, char *out, size_t out_size)
{
	char pattern[16];
	snprintf(pattern, sizeof(pattern), "%s,", cmd);

	char *p = strstr(resp, pattern);
	if (!p)
		return -1;

	p += strlen(pattern);
	char *end = strstr(p, ",OK");
	if (!end)
		return -1;

	cmd_copy_result(out, out_size, p, end);
	return 0;
}

int eric_write_eeprom_byte(struct sp_port *port, uint16_t base, uint16_t addr, uint8_t value)
{
	char cmd[32];
	snprintf(cmd, sizeof(cmd), "EEWR %04X %02X\r", base + addr, value);
	send_block(port, cmd, strlen(cmd));

	if (wait_for_answer(port, "EEWR,OK\r\r\n", 10, 0) != 0) {
		printf("EEPROM write failed at 0x%04X\n", addr);
		return -1;
	}

	// printf("EEPROM write success at 0x%04X\n", addr);
	return 0;
}

int eric_write_eeprom(struct sp_port *port, const char *filename, uint16_t base)
{
	size_t eeprom_size;
	uint8_t *data = load_file(filename, &eeprom_size);
	if (!data) {
		fprintf(stderr, "Error: cannot load EEPROM image %s\n", filename);
		return -1;
	}

	printf("Writing EEPROM (%zu bytes)...\n", eeprom_size);

	const int bar_width = 40; // width of progress bar
	for (uint16_t addr = 0; addr < eeprom_size; addr++) {
		uint8_t value = data[addr];

		if (eric_write_eeprom_byte(port, base, addr, value) < 0) {
			fprintf(stderr, "\nError writing EEPROM at 0x%04X (value=%02X)\n", addr, value);
			free(data);
			return -1;
		}

		// compute progress
		float progress = (float)(addr + 1) / eeprom_size;
		int pos = (int)(progress * bar_width);

		// print progress line
		printf("\rWrite 0x%04X = 0x%02X  [", base + addr, value);
		for (int i = 0; i < bar_width; i++)
			putchar(i < pos ? '#' : '-');
		printf("] %3.0f%%", progress * 100);
		fflush(stdout);
	}

	printf("\nEEPROM write complete (%zu bytes written)\n", eeprom_size);
	free(data);
	return 0;
}

int eric_read_eeprom_byte(struct sp_port *port, uint16_t base, uint16_t addr, uint8_t *value)
{
	char cmd[16];
	snprintf(cmd, sizeof(cmd), "EERE %04X 01\r", base + addr);
	send_block(port, cmd, strlen(cmd));

	uint8_t resp[64] = {0};
	int rcv_len = recv_block(port, resp, sizeof(resp));
	if (rcv_len <= 0)
		return -1;

	resp[rcv_len] = '\0';

	// Expected format: "EERE,00XX,OK"
	char *p = strstr((char *)resp, "EERE,");
	if (!p)
		return -1;

	p += 5; // skip "EERE,"

	uint32_t val = 0;
	if (sscanf(p, "%*2X%2X", &val) != 1)
		return -1;

	*value = (uint8_t)val;
	return 0;
}

int eric_read_eeprom_block(struct sp_port *port, uint16_t base, uint16_t addr, int read_size, uint8_t *out)
{
	char cmd[16];
	snprintf(cmd, sizeof(cmd), "EERE %04X %02X\r", base + addr, read_size);
	send_block(port, cmd, strlen(cmd));

	uint8_t resp[256] = {0};
	int rcv_len = recv_block(port, resp, sizeof(resp));
	if (rcv_len <= 0)
		return -1;

	resp[rcv_len] = '\0';

	// Find start of data after "EERE,"
	char *p = strstr((char *)resp, "EERE,");
	if (!p)
		return -1;

	p += 5; // skip "EERE,"

	// Parse comma-separated values
	int count = 0;
	char *token = strtok(p, ",");
	while (token && count < read_size) {
		if (strncmp(token, "OK", 2) == 0)
			break;

		// Expect format "00XX"
		unsigned int val;
		if (sscanf(token, "%*2X%2X", &val) == 1)
			out[count++] = (uint8_t)val;

		token = strtok(NULL, ",");
	}

	return (count == read_size) ? 0 : -1;
}

int eric_read_eeprom(struct sp_port *port, const char *filename, uint16_t base, size_t eeprom_size)
{
	FILE *f = fopen(filename, "wb");
	if (!f) {
		fprintf(stderr, "Error: cannot open %s for writing\n", filename);
		return -1;
	}

	const size_t read_size = 32; // read 32 bytes once a time
	uint8_t buffer[read_size];
	uint16_t addr = 0;
	int total = 0;

	printf("Reading EEPROM (%zu bytes)...\n", eeprom_size);

	for (size_t i = 0; i < eeprom_size; i += read_size) {
		if (eric_read_eeprom_block(port, base, addr, read_size, buffer) < 0) {
			fprintf(stderr, "\nError reading EEPROM at 0x%04X\n", addr);
			fclose(f);
			return -1;
		}

		fwrite(buffer, 1, read_size, f);

		addr += read_size;
		total += read_size;

		// progress indicator
		putchar('.');
		if ((i / read_size + 1) % 32 == 0)
			putchar('\n');
		fflush(stdout);
	}

	fclose(f);
	printf("\nEEPROM dump complete: %d bytes written to %s\n", total, filename);
	return 0;
}

static inline uint8_t swap_nibbles(uint8_t x) { return ((x & 0x0F) << 4) | (x >> 4); }

// --- Decode one 8-byte unlocking code ---
void eric_decode_splock(struct sp_port *port, uint16_t base, uint16_t addr, char *out)
{
	uint8_t buf[8] = {0};

	if (eric_read_eeprom_block(port, base, addr, sizeof(buf), buf) < 0) {
		fprintf(stderr, "Error reading EEPROM at 0x%04X\n", addr);
		strcpy(out, "????????");
		return;
	}

	out[0] = (char)(~buf[6]);
	out[1] = (char)(buf[5]);
	out[2] = (char)(buf[4]);
	out[3] = (char)(buf[0]);
	out[4] = (char)(~swap_nibbles(buf[7]));
	out[5] = (char)(swap_nibbles(buf[1]));
	out[6] = (char)(~buf[3]);
	out[7] = (char)(buf[2]);
	out[8] = '\0'; // null terminator
}

void eric_decode_phonelock(struct sp_port *port, uint16_t base, uint16_t addr, char *out)
{
	uint8_t buf[12] = {0};

	if (eric_read_eeprom_block(port, base, addr, sizeof(buf), buf) < 0) {
		fprintf(stderr, "Error reading EEPROM at 0x%04X\n", addr);
		strcpy(out, "????????");
		return;
	}

	int nolock = 1;
	for (int i = 0; i < 12; i++) {
		if (buf[i] != 0x00) {
			nolock = 0;
			break;
		}
	}

	if (nolock) {
		strcpy(out, "0000");
		return;
	}

	out[0] = (char)(~buf[7]);
	out[1] = (char)buf[6];
	out[2] = (char)buf[9];
	out[3] = (char)buf[1];
	out[4] = (char)(~swap_nibbles(buf[8]));
	out[5] = (char)swap_nibbles(buf[2]);
	out[6] = (char)(~buf[4]);
	out[7] = (char)buf[5];
	out[8] = '\0';
}

int eric_change_imei(struct sp_port *port, uint16_t base, uint16_t offset)
{
	uint8_t imei_data[18] = {0};

	for (size_t i = 0; i < sizeof(imei_data); i++) {
		eric_write_eeprom_byte(port, base, offset + i, imei_data[i]);
	}

	return 0;
}

int eric_get_imei_data(struct sp_port *port, struct phone_info *phone, uint16_t base, uint16_t addr)
{
	// printf("GD_IMEI_1: ");
	uint16_t GD_IMEI_1 = addr;
	uint8_t buffer[9] = {0};
	if (eric_read_eeprom_block(port, base, GD_IMEI_1, sizeof(buffer), buffer) < 0) {
		fprintf(stderr, "\nError reading EEPROM at 0x%04X\n", GD_IMEI_1);
	}
	for (int i = 0; i < 9; i++) {
		phone->gd_imei_1[i] = buffer[i];
	}

	// printf("GD_IMEI_2: ");
	uint16_t GD_IMEI_2 = addr + 9;
	if (eric_read_eeprom_block(port, base, GD_IMEI_2, sizeof(buffer), buffer) < 0) {
		fprintf(stderr, "\nError reading EEPROM at 0x%04X\n", GD_IMEI_2);
	}
	for (int i = 0; i < 9; i++) {
		phone->gd_imei_2[i] = buffer[i];
	}

	return 0;
}

void eric_z80_terminate(struct sp_port *port)
{
	printf("\nShutdown\n");
	send_block(port, "KILL\r", 5);
}
