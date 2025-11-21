#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <libserialport.h>

#include <core/common.h>
#include <core/io_ctx.h>
#include <core/payload.h>
#include <emp/common/avr_common.h>
#include <utils/big_math.h>

int emp_enter_boot(struct sp_port *port)
{
	uint8_t bignum_42_0_1[BIGNUM_SIZE] = {0};
	uint8_t bignum_43_0_1[BIGNUM_SIZE] = {0};
	uint8_t resp[8];
	int rcv_len;
	uint8_t a, b, c;

	// send {0x50, 0x01, 0xAA}
	a = 0x50, b = 0x01, c = 0xAA;
	uint8_t cmd50[3] = {a, b, c};
	send_block(port, cmd50, sizeof(cmd50));
	rcv_len = recv_block(port, resp, 3);
	if (rcv_len < 3) {
		fprintf(stderr, "Wrong reply\n");
		return -1;
	}

	//  send {0x42, 0x00, 0x01}
	a = 0x42, b = 0x00, c = 0x01;
	uint8_t cmd42[3] = {a, b, c};
	send_block(port, cmd42, sizeof(cmd42));
	send_block(port, bignum_42_0_1, sizeof(bignum_42_0_1));
	send_byte(port, 0xB8);
	rcv_len = recv_block(port, resp, 7);
	if (rcv_len < 7) {
		fprintf(stderr, "Wrong reply\n");
		return -1;
	}

	// send {0x43, 0x00, 0x01}
	a = 0x43, b = 0x00, c = 0x01;
	uint8_t cmd43[3] = {a, b, c};
	send_block(port, cmd43, sizeof(cmd43));
	send_block(port, bignum_43_0_1, sizeof(bignum_43_0_1));
	send_byte(port, 0xB7);
	rcv_len = recv_block(port, resp, 3);
	if (rcv_len <= 0) {
		fprintf(stderr, "Empty reply\n");
		return -1;
	}

	if (resp[2] != 0x92) {
		printf("FAILED\n");
		return -1;
	}

	printf("SUCCESS\n\n");

	return 0;
}

int emp_avr_bootup(struct sp_port *port, struct phone_info *phone)
{
	if (emp_enter_boot(port) != 0)
		return -1;

	send_byte(port, 'X');
	if (emp_avr_send_loader(port, "avr", ERIC_T39_BOOTLOADER) != 0)
		return -1;
	if (emp_avr_set_baudrate(port, 230400) != 0)
		return -1;
	if (emp_avr_get_imei(port, phone) != 0)
		return -1;
	if (emp_avr_get_ldr_v(port) != 0)
		return -1;

	return 0;
}

int emp_avr_send_flashloader(struct sp_port *port, struct phone_info *phone)
{
	if (emp_avr_init_cmd(port) != 0)
		return -1;
	if (emp_avr_send_loader(port, "avr", ERIC_T39_FLASHLOADER) != 0)
		return -1;
	if (emp_avr_set_baudrate(port, phone->baudrate) != 0)
		return -1;
	if (emp_avr_queue_cmd(port) != 0)
		return -1;
	if (emp_avr_get_ldr_v(port) != 0)
		return -1;

	return 0;
}

int emp_avr_send_gdfsloader(struct sp_port *port, struct phone_info *phone)
{
	if (emp_avr_init_cmd(port) != 0)
		return -1;
	if (emp_avr_send_loader(port, "avr", ERIC_T39_GDFSLOADER) != 0)
		return -1;
	if (emp_avr_set_baudrate(port, phone->baudrate) != 0)
		return -1;
	if (emp_avr_queue_cmd(port) != 0)
		return -1;

	return 0;
}
