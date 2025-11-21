#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <libserialport.h>

#include <core/common.h>
#include <core/connection.h>
#include <core/payload.h>
#include <core/serial.h>
#include <core/io_ctx.h>
#include <utils/big_math.h>

static int emp_avr_reconnect(struct sp_port *port, struct phone_info *phone)
{
	phone->avr_ignore_print = 1;
	serial_close(port);
	flush(port);
	printf("\n");
	sleep_ms(2500);

	/* reopen & handshake */
	if (serial_open(port) != 0)
		return -1;
	if (wait_for_Z(port, phone) != 0)
		return -1;
	if (send_question_mark(port, phone) != 0)
		return -1;

	return 0;
}

static const uint8_t big_array_1[BIGNUM_SIZE] = {0x6D, 0x4C, 0x4D, 0x5A, 0x73, 0xDF, 0xBB, 0xE0, 0x1A, 0x67, 0x0E,
                                                 0xA5, 0x5B, 0xC8, 0xE3, 0x5F, 0x83, 0x2A, 0x05, 0xB7, 0x6E, 0x0A,
                                                 0x22, 0x18, 0x36, 0xCB, 0x41, 0x44, 0x5C, 0x17, 0x73, 0xB2, 0xB5,
                                                 0x74, 0x52, 0xFB, 0x8D, 0xD5, 0xBF, 0xBE, 0x27, 0xB6, 0xA9, 0x96,
                                                 0xF0, 0xEB, 0xC1, 0x1E, 0xFA, 0x50, 0x54, 0x38, 0xD4, 0xE2, 0xC6,
                                                 0xBD, 0xD5, 0x5B, 0x51, 0x46, 0x0D, 0xAE, 0x6D, 0x60};

static const uint16_t big_array_2[] = {0x0003, 0x0005, 0x000B, 0x0013, 0x001F, 0x002B, 0x0035, 0x0043, 0x0047, 0x0061,
                                       0x0083, 0x0089, 0x009D, 0x00A3, 0x00BF, 0x00DF, 0x00EF, 0x0119, 0x0137, 0x014B};

static int bypass_boot = 0;
static uint8_t mul_res[MULREZ_SIZE];
static uint8_t bignumber_1[BIGNUM_SIZE];
static uint8_t bignumber_2[BIGNUM_SIZE];
static uint8_t bignumber_3[BIGNUM_SIZE];

static void emp_avr_boot4(void)
{
	memset(bignumber_1, 0, BIGNUM_SIZE);
	size_t bitpos = (D41C31D - 1);
	size_t byteidx = bitpos >> 3;
	size_t bitoff = bitpos & 7;
	if (byteidx < BIGNUM_SIZE) {
		bignumber_1[byteidx] = (uint8_t)(1u << bitoff);
	}

	memcpy(bignumber_2, mul_res, BIGNUM_SIZE);
	bignumber_2[BIGNUM_SIZE - 1] &= 0x7F;

	if (big_cmp(bignumber_2, bignumber_3, BIGNUM_SIZE) >= 0)
		big_sub(bignumber_2, bignumber_2, bignumber_3, BIGNUM_SIZE);

	ssize_t maxbit = (ssize_t)MULREZ_SIZE * 8;
	int found = 0;
	for (ssize_t byte = MULREZ_SIZE - 1; byte >= 0; --byte) {
		uint8_t val = mul_res[byte];
		if (val != 0) {
			for (int b = 7; b >= 0; --b) {
				if (val & (1u << b)) {
					maxbit = (ssize_t)(byte * 8 + b + 1); // number of bits
					found = 1;
					break;
				}
			}
		}
		if (found)
			break;
	}

	if (!found)
		maxbit = 0;

	size_t ebx = (D41C31D >> 3);
	uint8_t dl_mask = (uint8_t)(1u << (D41C31D & 7));
	for (ssize_t e = D41C31D; e < maxbit; ++e) {
		uint8_t tmp[BIGNUM_SIZE];
		big_double_mod(tmp, bignumber_1, bignumber_3, BIGNUM_SIZE);
		memcpy(bignumber_1, tmp, BIGNUM_SIZE);

		uint8_t al = 0;
		size_t mr_index = ebx;
		if (mr_index < MULREZ_SIZE)
			al = mul_res[mr_index];
		if (al & dl_mask) {
			uint8_t tmp2[BIGNUM_SIZE];
			big_add_mod(tmp2, bignumber_1, bignumber_2, bignumber_3, BIGNUM_SIZE);
			memcpy(bignumber_2, tmp2, BIGNUM_SIZE);
		}

		dl_mask <<= 1;
		if (dl_mask == 0) {
			ebx++;
			dl_mask = 1;
		}
	}
}

static void emp_avr_boot2(uint8_t *addr1, uint8_t *addr2, uint32_t currentrnd)
{
	uint8_t var_44[BIGNUM_SIZE];
	randomize();

	memcpy(bignumber_3, big_array_1, BIGNUM_SIZE);
	for (size_t i = 0; i < BIGNUM_SIZE; ++i)
		bignumber_2[i] = random1(0xFF);
	bignumber_2[BIGNUM_SIZE - 1] &= 0x3F;

	memcpy(bignumber_1, bignumber_2, BIGNUM_SIZE);
	memcpy(var_44, bignumber_2, BIGNUM_SIZE);

	big_mul(mul_res, bignumber_1, bignumber_2, BIGNUM_SIZE);

	memcpy(bignumber_2, mul_res, BIGNUM_SIZE);
	bignumber_2[BIGNUM_SIZE - 1] &= 0x7F;

	// call emp_avr_boot4
	emp_avr_boot4();

	for (int loop2count = 0; loop2count != 20; loop2count++) {
		int lowbit = (currentrnd & 1);
		currentrnd >>= 1;
		if (lowbit) {
			memset(bignumber_1, 0, BIGNUM_SIZE);

			uint16_t val = big_array_2[loop2count];
			bignumber_1[0] = (uint8_t)(val & 0xFF);
			bignumber_1[1] = (uint8_t)((val >> 8) & 0xFF);
			big_mul(mul_res, bignumber_1, bignumber_2, BIGNUM_SIZE);

			emp_avr_boot4();
		}
	}

	memcpy(addr2, bignumber_2, BIGNUM_SIZE);
	memcpy(addr1, var_44, BIGNUM_SIZE);
}

int emp_avr_bypass_boot_authority(struct sp_port *port, struct phone_info *phone)
{
	uint8_t bignum_42_0_1[BIGNUM_SIZE];
	uint8_t bignum_43_0_1[BIGNUM_SIZE];
	uint8_t bignum_43_0_1_tempstorage[BIGNUM_SIZE];
	int randomz[256];
	int randomznum = 0;
	int current_rnd = 0;
	int repeated_rnd = 0;
	uint8_t resp[8];

	int bootstrap_2d_step_done = 0;
	int bootstrap_3d_step_done = 0;
	uint32_t eqcount;
	uint8_t a = 0, b = 0, c = 0;

	memset(bignum_42_0_1, 0, BIGNUM_SIZE);
	bignum_42_0_1[BIGNUM_SIZE - 1] = 1;
	memset(bignum_43_0_1, 0, BIGNUM_SIZE);

	printf("Bootstrap First Step\n");
	for (int i = 0; i < 0x28; i++) {
		sleep_ms(50);

		printf("Try %d times\n", i + 1);

		if (bypass_boot != 0) {
			/* reopen & handshake */
			if (emp_avr_reconnect(port, phone) != 0)
				return -1;
		} else
			bypass_boot++;

		// send {0x50, 0x00, 0xAB}
		a = 0x50, b = 0x00, c = 0xAB;
		uint8_t cmd50[3] = {a, b, c};
		send_block(port, cmd50, sizeof(cmd50));
		if (recv_block(port, resp, 3) < 3) {
			fprintf(stderr, "Wrong reply\n");
			return -1;
		}

		//  send {0x42, 0, 1}
		a = 0x42, b = 0x00, c = 0x01;
		uint8_t cmd42[3] = {a, b, c};
		send_block(port, cmd42, sizeof(cmd42));
		send_block(port, bignum_42_0_1, BIGNUM_SIZE);
		uint8_t crc1 = simple_crc_add(bignum_42_0_1, BIGNUM_SIZE) + a + b + c;
		send_byte(port, crc1);
		if (recv_block(port, resp, 7) < 7) {
			fprintf(stderr, "Wrong reply\n");
			return -1;
		}
		current_rnd = (resp[3] << 16) | (resp[4] << 8) | resp[5];

		if (!bootstrap_2d_step_done) {
			eqcount = 0;
			for (int j = 0; j < randomznum; ++j) {
				if (randomz[j] == current_rnd)
					eqcount++;
			}
			if (eqcount > 1) {
				// do second step
				printf("Bootstrap Second Step ...\n");
				i = 0;
				emp_avr_boot2(bignum_43_0_1_tempstorage, bignum_42_0_1, current_rnd);
				repeated_rnd = current_rnd;
				bootstrap_2d_step_done++;
				printf("Bootstrap Third Step ...\n");
			}
			randomz[randomznum] = current_rnd;
			randomznum++;
		} else if (repeated_rnd == current_rnd) {
			memcpy(bignum_43_0_1, bignum_43_0_1_tempstorage, BIGNUM_SIZE);
			bootstrap_3d_step_done = 1;
			printf("Bootstrap Fourth Step ...\n");
		}

		// send {0x43, 0, 1}
		a = 0x43, b = 0x00, c = 0x01;
		uint8_t cmd43[3] = {a, b, c};
		send_block(port, cmd43, sizeof(cmd43));
		send_block(port, bignum_43_0_1, BIGNUM_SIZE);
		uint8_t crc2 = simple_crc_add(bignum_43_0_1, BIGNUM_SIZE) + a + b + c;
		send_byte(port, crc2);
		if (recv_block(port, resp, 3) < 3) {
			fprintf(stderr, "Empty reply\n");
			return -1;
		}

		if (!bootstrap_3d_step_done) {
			sleep_ms(50);
		} else {
			if (resp[2] != 0x92) {
				printf("FAILED\n");
				return -1;
			} else {
				// We are already bypassed rsa here =)
				printf("SUCCESS\n");
				return 0;
			}
		}
	}
	printf("FAILED\n");
	return -1;
}

static const char *avr_get_speed_val(int speed)
{
	switch (speed) {
	case 921600:
		return "C21";
	case 460800:
		return "C23";
	case 230400:
		return "C25";
	case 115200:
		return "C27";
	default:
		return NULL;
	}
}

int emp_avr_set_baudrate(struct sp_port *port, int baudrate)
{
	const char *cmd = avr_get_speed_val(baudrate);
	send_block(port, cmd, 3);

	if (serial_set_baudrate(port, baudrate) != 0)
		return -1;

	sleep_ms(500);
	flush(port);
	sleep_ms(500);

	return 0;
}

int emp_avr_init_cmd(struct sp_port *port)
{
	send_block(port, "0B", 2);

	return wait_for_byte(port, 'R');
}

int emp_avr_queue_cmd(struct sp_port *port)
{
	send_byte(port, 'Q');

	return wait_for_byte(port, '>');
}

int emp_avr_next_cmd(struct sp_port *port)
{
	send_byte(port, '\r');

	return wait_for_byte(port, '>');
}

void emp_avr_finish(struct sp_port *port)
{
	printf("\nShutdown\n");
	send_byte(port, 2);
	send_byte(port, 2);
	send_byte(port, 2);
	send_byte(port, 2);
}

int emp_avr_send_loader(struct sp_port *port, const char *dirname, const char *fname)
{
	char loader_name[512];
	snprintf(loader_name, sizeof(loader_name), "./loader/%s/%s", dirname, fname);

	size_t ldr_size;
	uint8_t *ldr = load_file(loader_name, &ldr_size);
	if (!ldr) {
		fprintf(stderr, "Cannot load %s\n", loader_name);
		return -1;
	}

	send_chunk(port, ldr, ldr_size);
	free(ldr);

	// --- Read response 0x0D 0x0A 0x3E
	if (wait_for_byte(port, '>') != 0)
		return -1;

	flush(port);
	return 0;
}

int emp_avr_get_imei(struct sp_port *port, struct phone_info *phone)
{
	send_byte(port, 'I');

	uint8_t resp[24] = {0};
	int rcv_len = recv_block(port, resp, sizeof(resp));
	if (rcv_len <= 0)
		return -1;

	memcpy(phone->otp_imei, resp, rcv_len - 3);
	phone->otp_imei[rcv_len - 3] = '\0';

	printf("IMEI: %s\n", phone->otp_imei);

	return 0;
}

int emp_avr_get_gdfs_imei(struct sp_port *port, struct phone_info *phone)
{
	send_byte(port, 'G');

	uint8_t resp[24] = {0};
	int rcv_len = recv_block(port, resp, sizeof(resp));
	if (rcv_len <= 0)
		return -1;

	memcpy(phone->gdfs_imei, resp, rcv_len - 3);
	phone->gdfs_imei[rcv_len - 3] = '\0';

	printf("IMEI: %s\n", phone->gdfs_imei);

	return 0;
}

int emp_avr_get_ldr_v(struct sp_port *port)
{
	send_byte(port, 'V');

	uint8_t resp[32] = {0};
	int rcv_len = recv_block(port, resp, sizeof(resp));
	if (rcv_len <= 0)
		return -1;

	char ldr_v[32] = {0};

	memcpy(ldr_v, resp, rcv_len - 3);
	ldr_v[rcv_len - 3] = '\0';

	printf("LDR: %s\n", ldr_v);

	return 0;
}

struct devmap {
	const char *name;
	const char *loader;
};

static const struct devmap map[] = {
        {"R520", ERIC_T39_GDFSLOADER},
        {"T39", ERIC_T39_GDFSLOADER},
        {"T65", ERIC_T39_GDFSLOADER},
        {"T68", ERIC_EMP_T68_GDFSLOADER},
        {"T200", ERIC_EMP_T200_GDFSLOADER},
        {"T230", ERIC_EMP_T2XX_T3XX_GDFSLOADER},
        {"T290", ERIC_EMP_T2XX_T3XX_GDFSLOADER},
        {"T300", ERIC_EMP_T2XX_T3XX_GDFSLOADER},
        {"T310", ERIC_EMP_T2XX_T3XX_GDFSLOADER},
        {"T610", ERIC_EMP_TZ6XX_GDFSLOADER},
        {"T630", ERIC_EMP_TZ6XX_GDFSLOADER},
        {"P800", ERIC_EMP_PXXX_GDFSLOADER},
        {"P900", ERIC_EMP_PXXX_GDFSLOADER},
        {"P910", ERIC_EMP_PXXX_GDFSLOADER},
        {"Z600", ERIC_EMP_TZ6XX_GDFSLOADER},
};

const char *emp_avr_get_gdfsloader(const char *device_name)
{
	for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
		if (!strcmp(device_name, map[i].name))
			return map[i].loader;
	}
	return NULL;
}

int emp_avr_flash_sbn(struct sp_port *port, const char *fname)
{
	flush(port);

	size_t fsize;
	uint8_t *data = load_file(fname, &fsize);
	if (!data || fsize < 4) {
		fprintf(stderr, "Invalid or empty SBN file\n");
		return -1;
	}

	printf("\nFlashing %s (%zu bytes)\n", fname, fsize);

	size_t x = 0;
	int reading = 1;

	int cnt = 0;

	while (reading && x + 2 < fsize) {
		if (memcmp(&data[x], "S003", 4) == 0) {
			// Send S003 and wait for acknowledgement
			send_block(port, "S003", 4);
			if (wait_for_byte(port, '!') != 0) {
				fprintf(stderr, "No ACK for S0\n");
				{
					free(data);
					return -1;
				}
			}
			sleep_ms(1000);

			x += 4;
		} else if (memcmp(&data[x], "S1", 2) == 0) {
			uint32_t address = (data[x + 3] << 8) | data[x + 4];
			uint8_t size = data[x + 2] + 3;

			send_block(port, &data[x], size);

			printf("\rWrite 0x%08X (%u bytes)", address, size);
			fflush(stdout);

			x += size;
		} else if (memcmp(&data[x], "S2", 2) == 0) {
			uint32_t address = (data[x + 3] << 16) | (data[x + 4] << 8) | data[x + 5];
			uint8_t size = data[x + 2] + 3;

			send_block(port, &data[x], size);

			printf("\rWrite 0x%08X (%u bytes)", address, size);
			fflush(stdout);

			x += size;
		} else if (memcmp(&data[x], "S8", 2) == 0) {
			reading = 0; // EOF
		} else {
			x++;
		}
		sleep_ms(10);
		cnt += 1;
	}
	send_block(port, "S8", 2);
	free(data);
	sleep_ms(5000);
	if (wait_for_byte(port, '>') != 0) {
		fprintf(stderr, "\nNo ACK for EOF (S8)\n");
		return -1;
	}
	flush(port);
	sleep_ms(2000);

	if (serial_set_baudrate(port, 115200) != 0)
		return -1;

	sleep_ms(500);
	// Finalize flash mode
	printf("\n\nFinalizing...\n");
	send_byte(port, 'E');
	sleep_ms(2000);
	if (wait_for_byte(port, '>') != 0) {
		fprintf(stderr, "No ACK for Final\n");
		return -1;
	}
	printf("OK\n");
	emp_avr_finish(port);

	return 0;
}

#define GDFS_DATA_SIZE 512
static uint8_t gdfs_cmd[GDFS_DATA_SIZE];

void avr_gdfs_send_block(struct sp_port *port, uint8_t ack, const uint8_t *data, uint16_t size)
{
	uint8_t outblock[256];
	uint16_t pos = 0;
	uint8_t bcrc = 0;

	if (size <= 12) {
		outblock[pos++] = ((size + 3) & 0x0F) | 0xE0;
	} else {
		outblock[pos++] = (((size + 3) >> 8) & 0x0F) | 0xF0;
		outblock[pos++] = (size + 3) & 0xFF;
	}

	outblock[pos++] = 0x4E;
	outblock[pos++] = 0x00;
	outblock[pos++] = size & 0xFF;

	// Copy payload and compute checksum
	for (uint16_t i = 0; i < size; i++) {
		uint8_t val = data[i];
		outblock[pos++] = val;
		bcrc ^= val;
	}

	// Append per-data checksum
	outblock[pos++] = bcrc;

	// Compute total CRC
	bcrc = 0;
	for (uint16_t i = 0; i < pos; i++)
		bcrc ^= outblock[i];
	//bcrc += 7;
	outblock[pos++] = bcrc;

	// Send sequence
	send_byte(port, ack);
	send_byte(port, ack + 7);
	send_block(port, outblock, pos);
}

/// Handle incoming GDFS frame and check signature
int avr_gdfs_resp(struct sp_port *port, uint8_t ack)
{
	memset(gdfs_cmd, 0, sizeof(gdfs_cmd));

	if (wait_for_byte(port, ack) != 0)
		return 0;
	if (wait_for_byte(port, ack + 7) != 0)
		return 0;

	int v = recv_byte(port);
	if (v < 0)
		return 0;

	uint8_t var2 = v;
	uint8_t var3 = 0;

	if (!(v & 0x10)) {
		var3 = (v & 0x0F) + 1;
	} else {
		int v2 = recv_byte(port);
		if (v2 < 0)
			return 0;
		var3 = (uint8_t)v2 + 1;
		var2 ^= (uint8_t)v2;
	}

	if (var3 > 0) {
		if (!recv_block(port, gdfs_cmd, var3))
			return 0;
		for (int i = 0; i < var3; i++)
			var2 ^= gdfs_cmd[i];
	}

	var2 += 7;

	if (wait_for_byte(port, var2) != 0)
		return 0;

	if (gdfs_cmd[0] != 'N')
		return 0;

	if (var3 - gdfs_cmd[2] != 4)
		return 0;

	return 1;
}

int getgdfs_44(struct sp_port *port)
{
	if (!avr_gdfs_resp(port, 0x60))
		return 0;
	return (gdfs_cmd[3] == 'D');
}

int getgdfs_45(struct sp_port *port)
{
	if (!avr_gdfs_resp(port, 0x60))
		return 0;
	return (gdfs_cmd[3] == 'E' && gdfs_cmd[4] == '0');
}

/// Main GDFS read routine
int avr_gdfs_write(struct sp_port *port, uint16_t addrinblock, uint8_t *data, uint16_t data_size)
{
	memset(gdfs_cmd, 0, sizeof(gdfs_cmd));

	gdfs_cmd[0] = 'W';
	gdfs_cmd[1] = (addrinblock >> 24) & 0xFF;
	gdfs_cmd[2] = (addrinblock >> 16) & 0xFF;
	gdfs_cmd[3] = (addrinblock >> 8) & 0xFF;
	gdfs_cmd[4] = addrinblock & 0xFF;

	gdfs_cmd[5] = (data_size >> 24) & 0xFF;
	gdfs_cmd[6] = (data_size >> 16) & 0xFF;
	gdfs_cmd[7] = (data_size >> 8) & 0xFF;
	gdfs_cmd[8] = data_size & 0xFF;

	memcpy(gdfs_cmd + 9, data, data_size); // empty data

	int cmd_len = 9 + data_size;

	avr_gdfs_send_block(port, 0x60, gdfs_cmd, cmd_len);

	if (!getgdfs_45(port))
		return 0x4202;

	if (!getgdfs_44(port))
		return 0x4203;

	return 0; // success
}

int avr_f_resetusercode(struct sp_port *port)
{
	send_byte(port, '_');
	send_byte(port, 'f');

	if (!avr_gdfs_resp(port, 0x60))
		return -1;

	uint8_t userlock_data[0xC] = {0};
	if (!avr_gdfs_write(port, 0xBB, userlock_data, sizeof(userlock_data)))
		return -1;

	return 0;
}
