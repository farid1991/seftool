#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <libserialport.h>

#include <core/common.h>
#include <core/connection.h>
#include <core/serial.h>
#include <core/io_ctx.h>
#include <emp/common/cmd.h>

#include "rtz_avr.h"
#include "rtz.h"

int rtz_arm_set_com_speed(struct sp_port *port, int baudrate)
{
	printf("Set arm port speed to %d...", baudrate);
	send_byte(port, STCMD_16_SETARMCOMSPEED);
	send_byte(port, rtz_avr_get_speed_val(baudrate));

	if (wait_for_byte(port, '>') != 0)
		return -1;

	printf("OK\n");
	return 0;
}

int rtz_arm_get_flash_id(struct sp_port *port)
{
	send_byte(port, STCMD_14_ARMCMDWITHREPLY);
	send_byte(port, 'I');

	uint8_t resp[4];
	int rcv_len = recv_block(port, resp, sizeof(resp));
	if (rcv_len <= 0)
		return -1;

	if (wait_for_byte(port, '>') != 0)
		return -2;

	uint16_t flash_mfr = get_half(&resp[0]);
	uint16_t flash_dev = get_half(&resp[2]);
	printf("ARM FlashID: 0x%X%X (%s)\n", flash_mfr, flash_dev, get_flash_name(flash_mfr));

	return 0;
}

int rtz_arm_write(struct sp_port *port, uint32_t arm_addr, uint32_t arm_len, const uint8_t *data)
{
	uint32_t acc_size;
	uint32_t write_size;
	uint32_t pad_size;

	while (arm_len > 0) {
		printf("\r[WRITE] 0x%08X", arm_addr);

		send_byte(port, STCMD_17_FLASH_ARM_BLOCK);
		send_half(port, arm_addr >> 8);

		// Get accepted block size (4 bytes)
		if (recv_word(port, &acc_size) != 0) {
			fprintf(stderr, "rtz_arm_write: failed to read accepted size");
			return -1;
		}

		write_size = (acc_size > arm_len) ? arm_len : acc_size;
		send_chunk(port, data, write_size);

		arm_addr += write_size;
		data += write_size;
		arm_len -= write_size;

		// pad remainder with 0xFF
		pad_size = acc_size - write_size;
		if (pad_size > 0) {
			uint8_t pad[256];
			memset(pad, 0xFF, sizeof(pad));
			while (pad_size > 0) {
				uint32_t chunk = (pad_size > sizeof(pad)) ? sizeof(pad) : pad_size;
				send_chunk(port, pad, chunk);
				pad_size -= chunk;
			}
		}

		// Expect “OK>”
		if (wait_for_answer(port, "OK>", 3, 0) != 0) {
			fprintf(stderr, "Invalid response\n");
			return -1;
		}
	}

	return 0;
}

int rtz_arm_flash(struct sp_port *port, const char *fw_name)
{
	size_t fsize;
	uint8_t *fw = load_file(fw_name, &fsize);
	if (!fw) {
		fprintf(stderr, "Error: failed to load firmware %s\n", fw_name);
		return -1;
	}

	if (fsize < 8) {
		fprintf(stderr, "Error: invalid firmware file (too small)\n");
		free(fw);
		return -1;
	}

	uint32_t arm_addr = *(uint32_t *)&fw[0];
	uint32_t arm_len = *(uint32_t *)&fw[4];

	if (arm_len + 8 != fsize) {
		fprintf(stderr, "Error: firmware size mismatch (len=%u, file=%zu)\n", arm_len, fsize);
		free(fw);
		return -1;
	}

	printf("\nFlashing ARM firmware: %s\n", fw_name);
	printf("Base Address: 0x%08X, Length: %u bytes, %u blocks\n", arm_addr, arm_len, (arm_len + 0xFFFF) >> 16);

	int res = rtz_arm_write(port, arm_addr, arm_len, fw + 8);
	free(fw);

	if (res == 0)
		printf("\n[OK] ARM flash completed successfully\n");
	else
		fprintf(stderr, "\n[ERROR] ARM flash failed (code %d)\n", res);

	return res;
}

int rtz_arm_send_bootloader(struct sp_port *port, const char *fname)
{
	printf("Sending arm-bootloader...");

	char loader_name[256];
	snprintf(loader_name, sizeof(loader_name), "./loader/rtz/%s", fname);

	size_t ldr_size;
	uint8_t *arm_bootloader = load_file(loader_name, &ldr_size);

	send_byte(port, STCMD_2_BOOTARM);
	send_half(port, ldr_size + 8 + 1);

	send_word(port, 0);
	send_byte(port, ldr_size);
	send_byte(port, ldr_size >> 8);
	send_byte(port, ldr_size >> 16);
	send_byte(port, ldr_size >> 24);
	send_block(port, arm_bootloader, ldr_size);
	send_byte(port, 'E'); // execute??
	free(arm_bootloader);

	// --- Read response B >
	if (wait_for_byte(port, '>') != 0)
		return -1;

	send_byte(port, STCMD_14_ARMCMDWITHREPLY);
	send_byte(port, 'H');
	if (rtz_wait_answer(port, "Ready", 5, 15 * TIMEOUT, 0) != 0)
		return -1;

	// --- Read response >
	if (wait_for_byte(port, '>') != 0)
		return -1;

	printf("OK\n");
	return 0;
}

int rtz_arm_init_boot(struct sp_port *port)
{
	send_byte(port, STCMD_1_ARMINIT);

	// --- Read response MR>
	if (wait_for_byte(port, '>') != 0)
		return -3;

	return 0;
}
