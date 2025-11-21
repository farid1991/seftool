#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <libserialport.h>

#include <core/common.h>
#include <core/connection.h>
#include <core/serial.h>
#include <core/io_ctx.h>
#include <core/payload.h>

#include <eric/common/eric_common.h>
#include "eric_avr.h"

#define EEPROM_SIZE 0x8000 // 32 Kbytes
#define READ_EEP_SIZE 0x100 // 256 bytes
#define READ_EEP_RESP 0x107 // 'R' + 0x100 data + checksum + 'O' 'K' '\r' '\n' '>'
#define WRITE_EEP_SIZE 0x20 // 32 bytes
#define WRITE_EEP_RESP 7 // 'I' + checksum + 'O' 'K' '\r' '\n' '>'

#define FLASH_BLOCK_SIZE 0x100
#define FLASH_SIZE (2 * 1024 * 1024) // 2 MB
#define FLASH_RESP_SIZE (FLASH_BLOCK_SIZE + 1) // 256 data + 1 checksum
#define TOTAL_BLOCKS (FLASH_SIZE / FLASH_BLOCK_SIZE)

int eric_t28_init_serv_mode(struct sp_port *port, struct phone_info *phone)
{
	if (eric_init_cmd(port) != 0)
		return -1;
	if (eric_new_handshake(port) != 0)
		return -1;
	if (eric_avr_send_loader(port, ERIC_T28_BOOTSTRAP) != 0)
		return -1;
	if (eric_avr_set_speed(port, phone->baudrate) != 0)
		return -1;
	if (eric_avr_get_imei(port, phone) != 0)
		return -1;

	return 0;
}

int eric_t28_vrn_ldr(struct sp_port *port)
{
	if (eric_init_cmd(port) != 0)
		return -1;

	if (eric_avr_send_loader(port, "t28_vrn.bin") != 0)
		return -1;

	send_byte(port, 'Q');
	uint8_t resp[0x16];
	if (serial_wait_packet(port, resp, 0xC, 5 * TIMEOUT) <= 0)
		return -1;

	resp[0xB] = '\0';
	printf("LDR: %s", resp);

	flush(port);

	uint8_t cmd[4];
	cmd[0] = 'B'; // BOOT?
	cmd[1] = 0x10; // LSB
	cmd[2] = 0x00; // MSB
	cmd[3] = 0x00;
	send_block(port, cmd, 4);

	if (serial_wait_packet(port, resp, sizeof(resp), 5 * TIMEOUT) <= 0)
		return -1;

	return 0;
}

int t28_read_eeprom(struct sp_port *port, uint16_t addr, uint8_t *out, uint8_t size)
{
	if (!out) {
		fprintf(stderr, "NULL output buffer passed to t28_read_eeprom\n");
		return -1;
	}

	// If size==0, device returns 256 bytes
	int resp_size = (size == 0) ? READ_EEP_SIZE : size;

	uint8_t cmd[4];
	cmd[0] = 'R';
	cmd[1] = addr & 0xFF; // LSB
	cmd[2] = addr >> 8; // MSB
	cmd[3] = size; // 0 = read 256 bytes

	// Response: 'R' + data + checksum + "OK\r\n>"
	uint8_t resp[resp_size + 7];

	// Send command
	send_block(port, cmd, sizeof(cmd));

	// Read full response
	size_t r = recv_block(port, resp, sizeof(resp));
	if (r != sizeof(resp)) {
		fprintf(stderr, "Error: incomplete response (%zu bytes)\n", r);
		return -1;
	}

	// Check header
	if (resp[0] != 'R') {
		fprintf(stderr, "Error: wrong response header %02X\n", resp[0]);
		return -1;
	}

	// Compute XOR checksum of data
	uint8_t calc = 0;
	for (int i = 0; i < resp_size; i++)
		calc ^= resp[1 + i];

	uint8_t chk = resp[1 + resp_size];
	if (chk != calc) {
		fprintf(stderr, "Checksum mismatch: got %02X expected %02X\n", chk, calc);
		return -1;
	}

	// Trailer must be "OK\r\n>"
	if (!(resp[2 + resp_size] == 'O' && resp[3 + resp_size] == 'K' && resp[4 + resp_size] == '\r' &&
	      resp[5 + resp_size] == '\n' && resp[6 + resp_size] == '>')) {
		fprintf(stderr, "Invalid OK trailer\n");
		return -1;
	}

	// Copy EEPROM data to user buffer
	memcpy(out, resp + 1, resp_size);

	return 0;
}

int t28_read_full_eeprom(struct sp_port *port, struct phone_info *phone)
{
	const int block_size = READ_EEP_SIZE; // 0x100
	const int iterations = EEPROM_SIZE / block_size;

	uint8_t *eeprom_data = malloc(EEPROM_SIZE);
	if (!eeprom_data) {
		fprintf(stderr, "Memory allocation failed\n");
		return -1;
	}

	printf("Reading EEPROM (%d blocks of 0x%X bytes)...\n", iterations, block_size);

	for (int block = 0; block < iterations; block++) {

		uint16_t addr = block * block_size; // block address

		// Read 256 bytes (size=0 means 256)
		int r = t28_read_eeprom(port, addr, eeprom_data + addr,
		                        0); // request 256-byte read

		if (r != 0) {
			fprintf(stderr, "\nBlock %d failed at address 0x%04X\n", block, addr);
			free(eeprom_data);
			return -1;
		}

		printf("Block %d/%d OK\r", block + 1, iterations);
		fflush(stdout);
	}

	printf("\nEEPROM read complete.\n");

	// Create filename
	char filename[256];
	snprintf(filename, sizeof(filename), "./backup/Ericsson_T28_%s.bin", phone->eep_imei);

	FILE *f = fopen(filename, "wb");
	if (!f) {
		perror("fopen");
		free(eeprom_data);
		return -1;
	}

	fwrite(eeprom_data, 1, EEPROM_SIZE, f);
	fclose(f);

	printf("Saved to %s\n", filename);

	free(eeprom_data);
	return 0;
}

int t28_write_eeprom(struct sp_port *port, uint16_t addr, const uint8_t *buf, uint8_t size)
{
	if (!buf) {
		fprintf(stderr, "NULL buffer passed to t28_write_eeprom\n");
		return -1;
	}

	if (size == 0 || size > 0x20) {
		fprintf(stderr, "Invalid write size %u (must be 1-32)\n", size);
		return -1;
	}

	if (addr + size > EEPROM_SIZE) {
		fprintf(stderr, "Address out of range: 0x%04X\n", addr);
		return -1;
	}

	uint8_t cmd[4];
	uint8_t resp[WRITE_EEP_RESP];

	// Build write command
	cmd[0] = 'I';
	cmd[1] = addr & 0xFF; // LSB
	cmd[2] = addr >> 8; // MSB
	cmd[3] = size; // number of data bytes

	// Send command header
	send_block(port, cmd, 4);

	// Send data (size bytes)
	send_block(port, buf, size);

	// Receive fixed 7-byte response
	int r = recv_block(port, resp, WRITE_EEP_RESP);
	if (r != WRITE_EEP_RESP) {
		fprintf(stderr, "Error: incomplete response (%d bytes)\n", r);
		return -1;
	}

	// Validate response header
	if (resp[0] != 'I') {
		fprintf(stderr, "Error: wrong response header 0x%02X\n", resp[0]);
		return -1;
	}

	// Compute checksum
	uint8_t check = 0;
	for (uint8_t i = 0; i < size; i++)
		check ^= buf[i];

	// Compare with returned checksum
	if (resp[1] != check) {
		fprintf(stderr, "Checksum mismatch: got %02X expected %02X\n", resp[1], check);
		return -1;
	}

	// Validate "OK\r\n>"
	if (!(resp[2] == 'O' && resp[3] == 'K' && resp[4] == '\r' && resp[5] == '\n' && resp[6] == '>')) {
		fprintf(stderr, "Error: invalid OK tail (%02X %02X %02X %02X %02X)\n", resp[2], resp[3], resp[4],
		        resp[5], resp[6]);
		return -1;
	}

	return 0; // success
}

int t28_write_full_eeprom(struct sp_port *port, const char *eep_name)
{
	size_t eep_size;
	uint8_t *eeprom = load_file(eep_name, &eep_size);
	if (!eeprom) {
		fprintf(stderr, "Cannot open eeprom %s\n", eep_name);
		return -1;
	}

	if (eep_size != EEPROM_SIZE) {
		fprintf(stderr, "Wrong eeprom size %zu\n", eep_size);
		free(eeprom);
		return -1;
	}

	int pages = EEPROM_SIZE / WRITE_EEP_SIZE;
	printf("Writing EEPROM (%d pages)...\n", pages);

	for (uint16_t addr = 0; addr < EEPROM_SIZE; addr += WRITE_EEP_SIZE) {

		int r = t28_write_eeprom(port, addr, eeprom + addr, WRITE_EEP_SIZE);
		if (r != 0) {
			fprintf(stderr, "\nError writing page at 0x%04X\n", addr);
			free(eeprom);
			return -1;
		}

		printf("Page %04X OK\r", addr);
		fflush(stdout);
	}

	printf("\nEEPROM write complete.\n");
	free(eeprom);
	return 0;
}

int t28_read_flash(struct sp_port *port, struct phone_info *phone)
{
	printf("Reading FLASH (%d Kbytes %d blocks)...\n", FLASH_SIZE / 1024, TOTAL_BLOCKS);

	uint8_t *flash = malloc(FLASH_SIZE);
	if (!flash)
		return -1;

	send_byte(port, 'f');
	if (recv_byte(port) != 'f') {
		fprintf(stderr, "Error: flash init step 1 failed\n");
		free(flash);
		return -1;
	}

	send_byte(port, ' ');
	if (recv_byte(port) != 'R') {
		fprintf(stderr, "Error: flash init step 2 failed\n");
		free(flash);
		return -1;
	}

	uint8_t resp[FLASH_RESP_SIZE];
	int offset = 0;

	for (int block = 0; block < TOTAL_BLOCKS; block++) {
		// Request next 256-byte block
		send_byte(port, 'S');

		int len = recv_block(port, resp, sizeof(resp));

		if (len != FLASH_RESP_SIZE) {
			fprintf(stderr, "\nError: incorrect response size (%d)\n", len);
			free(flash);
			return -1;
		}

		uint8_t calc = 0;
		for (int i = 0; i < FLASH_BLOCK_SIZE; i++)
			calc ^= resp[i];

		uint8_t chk = resp[FLASH_BLOCK_SIZE];

		if (chk != calc) {
			fprintf(stderr, "\nError: checksum mismatch at block %d (got %02X expected %02X)\n", block, chk,
			        calc);
			free(flash);
			return -1;
		}

		// Copy block into big flash buffer
		memcpy(flash + offset, resp, FLASH_BLOCK_SIZE);
		offset += FLASH_BLOCK_SIZE;

		printf("\rBlock %d/%d OK", block + 1, TOTAL_BLOCKS);
		fflush(stdout);
	}

	if (wait_for_answer(port, "OK\r\n>", 5, 0) != 0) {
		fprintf(stderr, "\nError: missing OK after flash read\n");
		free(flash);
		return -1;
	}

	printf("\nFLASH read complete.\n");

	char filename[256];
	snprintf(filename, sizeof(filename), "Ericsson_T28_Flash_%s.bin", phone->eep_imei);

	FILE *fp = fopen(filename, "wb");
	if (!fp) {
		perror("fopen");
		free(flash);
		return -1;
	}

	fwrite(flash, 1, FLASH_SIZE, fp);
	fclose(fp);

	printf("Saved to %s\n", filename);

	free(flash);
	return 0;
}

int eric_t28_read_eeprom(struct sp_port *port, struct phone_info *phone)
{
	if (eric_t28_init_serv_mode(port, phone) != 0)
		return -1;

	if (eric_avr_send_byte(port, '\r') != 0)
		return -1;

	flush(port);

	if (eric_t28_vrn_ldr(port) != 0)
		return -1;

	if (t28_read_full_eeprom(port, phone) != 0)
		return -1;

	send_byte(port, 'Q');

	return 0;
}

int eric_t28_write_eeprom(struct sp_port *port, struct phone_info *phone, const char *eep_name)
{
	if (eric_t28_init_serv_mode(port, phone) != 0)
		return -1;

	if (eric_avr_send_byte(port, '\r') != 0)
		return -1;

	flush(port);

	if (eric_t28_vrn_ldr(port) != 0)
		return -1;

	if (t28_write_full_eeprom(port, eep_name) != 0)
		return -1;

	send_byte(port, 'Q');

	return 0;
}

int eric_t28_read_flash(struct sp_port *port, struct phone_info *phone)
{
	if (eric_t28_init_serv_mode(port, phone) != 0)
		return -1;

	if (eric_avr_send_byte(port, '\r') != 0)
		return -1;

	flush(port);

	if (eric_t28_vrn_ldr(port) != 0)
		return -1;

	if (t28_read_flash(port, phone) != 0)
		return -1;

	send_byte(port, 'Q');

	return 0;
}

int eric_t28_identify(struct sp_port *port, struct phone_info *phone)
{
	if (eric_t28_init_serv_mode(port, phone) != 0)
		return -1;

	if (eric_avr_send_byte(port, '\r') != 0)
		return -1;

	flush(port);

	if (eric_t28_vrn_ldr(port) != 0)
		return -1;

	// uint8_t buffer[32];
	// if (t28_read_eeprom(port, 0, buffer, sizeof(buffer)) != 0)
	// 	return -1;

	// for (size_t i = 0; i < sizeof(buffer); i++) {
	// 	printf("%02X ", buffer[i]);
	// }
	// printf("\n");

	send_byte(port, 'Q');

	return 0;
}
