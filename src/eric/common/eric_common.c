#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <libserialport.h>

#include <core/common.h>
#include <core/connection.h>
#include <core/serial.h>
#include <core/io_ctx.h>

int eric_new_handshake(struct sp_port *port)
{
	uint8_t hdr[2] = {0, 3};
	send_block(port, hdr, sizeof(hdr));

	uint16_t res16;
	recv_half(port, &res16);

	uint8_t empty_arr[64] = {0};
	send_block(port, empty_arr, sizeof(empty_arr));
	uint32_t res24;
	recv_mediumword(port, &res24);

	send_block(port, empty_arr, sizeof(empty_arr));

	printf("Wait for 'S'... ");

	uint8_t resp;
	int rcv_len = serial_wait_packet(port, &resp, 1, 1000 * TIMEOUT);
	if (rcv_len <= 0 || resp != 'S') {
		printf("Failed\n");
		return -1;
	}

	printf("OK\n");
	return 0;
}

const char *eric_get_speed_val(int speed)
{
	switch (speed) {
	case 9600:
		return "C01";
	case 19200:
		return "C47";
	case 38400:
		return "C45";
	case 57600:
		return "C2D";
	case 115200:
		return "C27";
	case 230400:
		return "C25";
	default:
		return NULL;
	}
}

int eric_init_cmd(struct sp_port *port)
{
	send_block(port, "0B", 2);

	return wait_for_byte(port, 'R');
}
