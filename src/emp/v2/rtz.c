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
#include <emp/common/avr_common.h>
#include <emp/common/cmd.h>
#include <eric/common/eric_common.h>
#include <utils/big_math.h>

#include "rtz_arm.h"
#include "rtz_avr.h"
#include "rtz.h"

int rtz_wait_answer(struct sp_port *port, const char *expected, size_t len, int timeout_ms, int skiperrors)
{
	uint8_t *buf = malloc(len);
	size_t received = 0;

	while (received < len) {
		int rcv_len = serial_read(port, buf + received, len - received, timeout_ms);
		if (rcv_len <= 0) {
			fprintf(stderr, "Read failed (rcv_len=%d)\n", rcv_len);
			return -1;
		}
		received += rcv_len;
	}

	if (memcmp(buf, expected, len) != 0) {
		if (skiperrors != 1)
			fprintf(stderr, "[rtz_wait_answer] Unexpected reply: %.*s (expected %s)\n", 3, buf, expected);

		free(buf);
		return -1;
	}

	free(buf);
	return 0;
}

static int rtz_send_qhldr(struct sp_port *port, const char *fname, int *rsa_active)
{
	char loader_name[256];
	snprintf(loader_name, sizeof(loader_name), "./loader/rtz/%s", fname);

	size_t qhldr_size;
	uint8_t *qhldr = load_file(loader_name, &qhldr_size);

	avr_bin_t *avr = (avr_bin_t *)qhldr;

	size_t qh_size = sizeof(avr_bin_t);
	size_t qa_size = avr->prologue_size0;
	size_t qd_size = avr->payload_size0;

	uint8_t *qh00 = qhldr;
	uint8_t *qa00 = qhldr + qh_size;
	uint8_t *qd00 = qhldr + qh_size + qa_size;

	// RSA active
	*rsa_active = 1;

	// --- Send header ...
	send_block(port, "QH00", 4);
	if (rtz_wait_answer(port, "EsB", 3, 15 * TIMEOUT, 1) < 0) {
		// RSA inactive
		*rsa_active = 0;
		free(qhldr);
		return 0;
	}

	send_chunk(port, qh00, qh_size);
	if (rtz_wait_answer(port, "EhM", 3, 15 * TIMEOUT, 1) < 0) {
		// RSA inactive
		*rsa_active = 0;
		free(qhldr);
		return 0;
	}

	// --- Send prologue
	send_block(port, "QA00", 4);
	send_chunk(port, qa00, qa_size);
	if (rtz_wait_answer(port, "EaT", 3, 15 * TIMEOUT, 1) < 0) {
		// RSA inactive
		*rsa_active = 0;
		free(qhldr);
		return 0;
	}

	// --- Send body
	send_block(port, "QD00", 4);
	send_chunk(port, qd00, qd_size);
	if (rtz_wait_answer(port, "EdQ", 3, 15 * TIMEOUT, 1) < 0) {
		// RSA inactive
		*rsa_active = 0;
		free(qhldr);
		return 0;
	}

	free(qhldr);

	if (wait_for_byte(port, '>') != 0)
		return -1;

	return 0;
}

int rtz_setool_rsabypass(struct sp_port *port)
{
	const char bypass[64] = "./loader/rtz/rsabypass.bin";

	size_t fsize;
	uint8_t *buffer = load_file(bypass, &fsize);
	if (!buffer) {
		fprintf(stderr, "can't read %s\n", bypass);
		return -1;
	}

	uint8_t cmd_buf[0x800];
	int cmd_len = cmd_encode_binary_packet(0x23, buffer, fsize, cmd_buf);
	free(buffer);

	if (cmd_len <= 0)
		return -1;

	// --- send cmd23 command
	if (serial_send_packetdata_ack(port, cmd_buf, cmd_len) < 0)
		return -1;

	uint8_t resp[2];
	int rcv_len = serial_wait_packet(port, resp, sizeof(resp), 5 * TIMEOUT);
	if (rcv_len <= 0)
		return -1;

	return 0;
}

int rtz_boot_up(struct sp_port *port, struct phone_info *phone)
{
	printf("Check if RSA is active\n");
	int rsa_active;
	if (rtz_send_qhldr(port, ERIC_EMP_RTZ_QHLDR, &rsa_active) != 0)
		return -1;

	if (rsa_active) {
		printf("RSA Active, bypass RSA\n");

		if (emp_avr_set_baudrate(port, phone->baudrate) != 0)
			return -1;
		if (emp_avr_get_imei(port, phone) != 0)
			return -1;

		if (emp_avr_init_cmd(port) != 0)
			return -1;
		if (emp_avr_send_loader(port, "rtz", ERIC_EMP_RTZ_RECOVERY_LOADER) != 0)
			return -1;

		if (rtz_avr_activate_loader(port, phone) != 0)
			return -1;
		if (rtz_setool_rsabypass(port) != 0)
			return -1;

		goto succedd;
	}

	printf("Bypass boot authority\n");
	if (emp_avr_bypass_boot_authority(port, phone) != 0)
		return -1;

	if (connection_set_speed(port, phone) != 0)
		return -1;

succedd:
	send_byte(port, 'X');

	return 0;
}
