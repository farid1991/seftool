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

int eric_avr_set_speed(struct sp_port *port, int baudrate)
{
	if (baudrate > 230400)
		baudrate = 230400;

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

int eric_avr_send_byte(struct sp_port *port, uint8_t cmd)
{
	send_byte(port, cmd);

	return wait_for_byte(port, '>');
}

int eric_avr_send_loader(struct sp_port *port, const char *fname)
{
	printf("Sending loader... ");

	char loader_name[512];
	snprintf(loader_name, sizeof(loader_name), "./loader/avr/%s", fname);

	size_t ldr_size;
	uint8_t *ldr = load_file(loader_name, &ldr_size);
	if (!ldr) {
		fprintf(stderr, "Cannot open loader %s\n", loader_name);
		return -1;
	}

	// Send data block
	serial_write_chunks(port, ldr, ldr_size, 0x100);

	free(ldr);

	if (wait_for_byte(port, '>') != 0)
		return -1;

	printf("OK\n");

	return 0;
}

int eric_avr_get_imei(struct sp_port *port, struct phone_info *phone)
{
	send_byte(port, 'I');

	uint8_t resp[40] = {0};
	int rcv_len = recv_block(port, resp, sizeof(resp));
	if (rcv_len <= 0)
		return -1;

	resp[rcv_len] = '\0';

	// Convert to string for parsing
	char *str = (char *)resp;

	// Skip leading "\r\n"
	char *start = str + 2;

	// Find trailing "\r\n>"
	char *end = strstr(start, "\r\n>");
	if (end)
		*end = '\0'; // terminate before it

	// Copy to phone->eep_imei
	strncpy(phone->eep_imei, start, sizeof(phone->eep_imei) - 1);
	phone->eep_imei[sizeof(phone->eep_imei) - 1] = '\0';

	printf("IMEI: %s\n", phone->eep_imei);

	return 0;
}
