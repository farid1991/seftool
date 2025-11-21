#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <libserialport.h>

#include <core/common.h>
#include <core/serial.h>
#include <core/payload.h>
#include <emp/common/cmd.h>

#include "babe.h"
#include "connection.h"
#include "csloader.h"
#include "loader.h"
#include "gdfs.h"
#include "break.h"

static int loader_type = 0;

int loader_get_hello(struct packetdata_t *packet)
{
	char loader_hello[256];
	if (packet->length >= sizeof(loader_hello))
		return -1;

	memcpy(loader_hello, packet->data, packet->length);
	loader_hello[packet->length] = '\0';

	printf("LDR: %s\n", loader_hello);

	loader_type = LDR_UNKNOWN;

	if (strstr(loader_hello, "CS_LOADER") || strstr(loader_hello, "CSLOADER")) {
		// printf("This is a CHIPSELECT loader\n");
		loader_type = LDR_CHIPSELECT;
	} else if (strstr(loader_hello, "FILESYSTEMLOADER") || strstr(loader_hello, "FILE_SYSTEM_LOADER") ||
	           strstr(loader_hello, "FS_LOADER")) {
		// printf("This is a FILESYSTEM loader\n");
		loader_type = LDR_CHIPSELECT;
	} else if (strstr(loader_hello, "PRODUCTION_ID") || strstr(loader_hello, "PRODUCTIONID") ||
	           strstr(loader_hello, "PRODUCTION_LOADER")) {
		// printf("This is a PRODUCTION_ID loader\n");
		loader_type = LDR_PRODUCT_ID;
	} else if (strstr(loader_hello, "CERTLOADER")) {
		// printf("This is a CERTIFICATE loader\n");
		loader_type = LDR_CERT;
	} else if (strstr(loader_hello, "FLASHLOADER")) {
		// printf("This is a FLASH loader\n");
		loader_type = LDR_FLASH;
	} else if (strstr(loader_hello, "MEM_PATCHER")) {
		// printf("This is a MEM_PATCHER loader\n");
		loader_type = LDR_FLASH;
	} else if (strstr(loader_hello, "patched")) {
		// printf("This is a patched loader\n");
		loader_type = LDR_FLASH;
	} else {
		// printf("%s\nThis is an unknown loader type\n", loader_hello);
		loader_type = LDR_UNKNOWN;
	}

	if (strstr(loader_hello, "SETOOL"))
		printf("Let's say thanks to the_laser =)\n");

	if (strstr(loader_hello, "den_po"))
		printf("Let's say thanks to den_po =)\n");

	return 0;
}

int loader_send_binary_cmd3e(struct sp_port *port, const char *fname)
{
	char loader_name[512];
	snprintf(loader_name, sizeof(loader_name), "./loader/%s", fname);

	size_t fsize;
	uint8_t *buffer = load_file(loader_name, &fsize);
	if (!buffer) {
		fprintf(stderr, "can't read %s\n", loader_name);
		return -1;
	}

	uint8_t cmd_buf[0x800];
	int cmd_len = cmd_encode_binary_packet(0x3E, buffer, fsize, cmd_buf);
	if (cmd_len <= 0) {
		free(buffer);
		return -1;
	}
	free(buffer);

	// --- send cmd3e command
	if (serial_send_packetdata_ack(port, cmd_buf, cmd_len) < 0)
		return -1;

	// --- Read hello response from loader =)
	uint8_t hello_buf[128];
	int rcv_len = serial_wait_packet(port, hello_buf, sizeof(hello_buf), 3 * TIMEOUT);
	if (rcv_len <= 0)
		return -1;

	if (hello_buf[1] == 0xFC && hello_buf[2] == 0xFF) // CMD3E DB2000 DB2010 CID29 from SETOOL
	{
		printf("Break CMD3E\n");
		return 0;
	}

	struct packetdata_t repl;
	if (cmd_decode_packet(hello_buf, rcv_len, &repl) != 0)
		return -1;

	loader_get_hello(&repl);

	return 0;
}

int loader_send_unsigned_bin(struct sp_port *port, const char *fname, uint32_t ram_addr)
{
	char loader_name[512];
	snprintf(loader_name, sizeof(loader_name), "./loader/%s", fname);

	size_t fsize;
	uint8_t *buffer = load_file(loader_name, &fsize);
	if (!buffer) {
		fprintf(stderr, "can't read %s\n", loader_name);
		return -1;
	}

	// --- Build ram_addr packet (little endian)
	uint8_t addr_packet[4];
	addr_packet[0] = (uint8_t)(ram_addr & 0xFF);
	addr_packet[1] = (uint8_t)((ram_addr >> 8) & 0xFF);
	addr_packet[2] = (uint8_t)((ram_addr >> 16) & 0xFF);
	addr_packet[3] = (uint8_t)((ram_addr >> 24) & 0xFF);

	if (serial_write(port, addr_packet, sizeof(addr_packet)) < 0)
		goto exit_error;

	// --- Build payload size packet (little endian)
	uint8_t size_packet[4];
	size_packet[0] = (uint8_t)(fsize & 0xFF);
	size_packet[1] = (uint8_t)((fsize >> 8) & 0xFF);
	size_packet[2] = (uint8_t)((fsize >> 16) & 0xFF);
	size_packet[3] = (uint8_t)((fsize >> 24) & 0xFF);

	if (serial_write(port, size_packet, sizeof(size_packet)) < 0)
		goto exit_error;

	// --- Send loader body
	if (serial_write_chunks(port, buffer, fsize, 0x4000) < 0)
		goto exit_error;

	// --- Read hello response from loader =)
	uint8_t hello_buf[128];
	int rcv_len = serial_wait_packet(port, hello_buf, sizeof(hello_buf), 3 * TIMEOUT);
	if (rcv_len <= 0)
		return -1;

	struct packetdata_t repl;
	if (cmd_decode_packet(hello_buf, rcv_len, &repl) != 0)
		return -1;

	loader_get_hello(&repl);

	free(buffer);
	return 0;

exit_error:
	free(buffer);
	return -1;
}

// --- Loader activate
int loader_activate_payload(struct sp_port *port, struct phone_info *phone)
{
	if (loader_type == LDR_CHIPSELECT) {
		uint8_t cmd_buf[64];
		uint8_t resp[128];
		struct packetdata_t repl = {0};

		int cmd_len, rcv_len;

		// --- Activate CS loader
		printf("Activating CHIPSELECT loader... ");
		cmd_len = cmd_encode_csloader_packet(CMD_CSLOADER, 0x09, NULL, 0, cmd_buf);
		if (cmd_len <= 0)
			return -1;
		if (serial_send_packetdata_ack(port, cmd_buf, cmd_len) < 0)
			return -1;

		rcv_len = serial_wait_packet(port, resp, 8, 20 * TIMEOUT);
		if (rcv_len <= 0)
			return -1;

		if (cmd_decode_packet(resp, rcv_len, &repl) != 0)
			return -1;

		if (repl.data[1] != 0x00) {
			fprintf(stderr, "failed activating loader\n");
			return -1;
		}
		printf("activated\n");

		if (phone->gdfs_server == 0)
			return 0;

		// --- Activate GDFS server
		printf("Activating GDFS server... ");
		cmd_len = cmd_encode_csloader_packet(CMD_CS_GDFS, CMD_GDFS_ACTIVATION, NULL, 0, cmd_buf);
		if (cmd_len <= 0)
			return -1;

		if (serial_send_packetdata_ack(port, cmd_buf, cmd_len) < 0)
			return -1;

		rcv_len = serial_wait_packet(port, resp, 8, 500 * TIMEOUT);
		if (rcv_len <= 0)
			return -1;

		if (cmd_decode_packet(resp, rcv_len, &repl) != 0)
			return -1;

		if (repl.data[1] != 0x00) {
			fprintf(stderr, "failed\n");
			return -1;
		}
		printf("activated\n");

		// --- Test unlock command should print phone name if unlocked
		printf("Test GDFS Server... ");
		if (csloader_gdfs_get_phonename(port, phone) < 0) {
			printf("locked\n");
			return -1;
		}
		printf("unlocked\n");
		printf("%s ", phone->phone_name);

		// No cxc_article for DB2000 & DB2010(0x8000) =)
		if (phone->chip_id == DB2000 || phone->chip_id == DB2010_1) {
			printf("\n\n");
			return 0;
		}

		if (csloader_gdfs_get_cxc_article(port, phone) < 0)
			return -1;

		printf("%s\n\n", phone->cxc_article);
		strncpy(phone->anycid_target, phone->cxc_article, sizeof(phone->cxc_article));

		return 0;
	}

	// ------------------- Non-CS Loader -------------------

	// Get Flash data
	if (loader_get_flash_info(port, phone) != 0)
		return -1;

	// Get OTP data
	if (loader_get_otp_info(port, phone) != 0)
		return -1;

	return 0;
}

int loader_send_qhtry_jdflasher(struct sp_port *port, struct phone_info *phone, const char *fname)
{
	char loader_name[512];
	snprintf(loader_name, sizeof(loader_name), "./loader/%s", fname);

	size_t fsize;
	uint8_t *buffer = load_file(loader_name, &fsize);
	if (!buffer) {
		fprintf(stderr, "can't read %s\n", loader_name);
		return -1;
	}

	struct babehdr_t *babehdr = (struct babehdr_t *)buffer;

	size_t qh_size = sizeof(struct babehdr_t);
	size_t qa_size = babehdr->prologuesize1;

	uint8_t *qh00 = buffer;
	uint8_t *qa00 = buffer + qh_size;

	// --- Send QH00
	// printf("Send header ...\n");
	if (serial_write(port, (uint8_t *)"QH00", 4) < 0)
		goto exit_error;
	if (serial_wait_answer(port, "EsB", 5 * TIMEOUT, 1) < 0)
		goto exit_error;
	if (serial_write(port, qh00, qh_size) < 0)
		goto exit_error;
	if (serial_wait_answer(port, "EhM", 5 * TIMEOUT, 1) < 0)
		goto exit_error;

	// --- Send QA00
	// printf("Send prologue ...\n");
	if (serial_write(port, (uint8_t *)"QA00", 4) < 0)
		goto exit_error;
	if (serial_write_chunks(port, qa00, qa_size, 0x400) < 0)
		goto exit_error;
	if (serial_wait_answer(port, "EaT", 5 * TIMEOUT, 1) < 0)
		goto exit_error;

	// break-rsa
	uint8_t byteleft;
	int rcv_len = serial_read(port, &byteleft, 1, 5 * TIMEOUT);
	if (rcv_len <= 0)
		goto exit_error;

	printf("STARTING JDFLASHER BOOTLOADER...\n");
	if (serial_write(port, (uint8_t *)"R", 1) < 0)
		goto exit_error;

	sp_flush(port, SP_BUF_BOTH);

	uint8_t hello_buf[64];
	rcv_len = serial_wait_packet(port, hello_buf, sizeof(hello_buf), 20 * TIMEOUT);
	if (rcv_len <= 0)
		goto exit_error;

	struct packetdata_t repl;
	if (cmd_decode_packet(hello_buf, rcv_len, &repl) != 0)
		goto exit_error;

	loader_get_hello(&repl);

	free(buffer);

	// Activate loader
	if (loader_activate_payload(port, phone) != 0)
		return -1;

	phone->qhldr_sent = 1;

	return 0;

exit_error:
	free(buffer);
	return -1;
}

int loader_send_qhtry_setool(struct sp_port *port, struct phone_info *phone, const char *fname)
{
	char loader_name[512];
	snprintf(loader_name, sizeof(loader_name), "./loader/%s", fname);

	size_t fsize;
	uint8_t *buffer = load_file(loader_name, &fsize);
	if (!buffer) {
		fprintf(stderr, "can't read %s\n", loader_name);
		return -1;
	}

	struct babehdr_t *babehdr = (struct babehdr_t *)buffer;

	size_t qh_size = sizeof(struct babehdr_t);
	size_t qa_size = babehdr->prologuesize1;
	size_t qd_size = babehdr->payloadsize1;

	uint8_t *qh00 = buffer;
	uint8_t *qa00 = buffer + qh_size;
	uint8_t *qd00 = buffer + qh_size + qa_size;

	// --- Send QH00
	// printf("Send header ...\n");
	if (serial_write(port, (uint8_t *)"QH00", 4) < 0)
		goto exit_error;
	if (serial_wait_answer(port, "EsB", 5 * TIMEOUT, 1) < 0)
		goto exit_error;
	if (serial_write(port, qh00, qh_size) < 0)
		goto exit_error;
	if (serial_wait_answer(port, "EhM", 5 * TIMEOUT, 1) < 0)
		goto exit_error;

	// --- Send QA00
	// printf("Send prologue ...\n");
	if (serial_write(port, (uint8_t *)"QA00", 4) < 0)
		goto exit_error;
	if (serial_write_chunks(port, qa00, qa_size, 0x400) < 0)
		goto exit_error;
	if (serial_wait_answer(port, "EaT", 5 * TIMEOUT, 1) < 0)
		goto exit_error;
	if (serial_wait_answer(port, "EbS", 5 * TIMEOUT, 1) < 0)
		goto exit_error;

	// --- Send QD00
	// printf("Send body ...\n");
	if (serial_write(port, (uint8_t *)"QD00", 4) < 0)
		goto exit_error;
	if (serial_write_chunks(port, qd00, qd_size, 0x400) < 0)
		goto exit_error;
	if (serial_wait_answer(port, "EdQ", 5 * TIMEOUT, 1) < 0)
		goto exit_error;

	free(buffer);

	// break-rsa or anycid exploit
	uint8_t byteleft;
	int rcv_len = serial_read(port, &byteleft, 1, 3 * TIMEOUT);
	if (rcv_len <= 0)
		return -1;

	printf("STARTING SETOOL BOOTLOADER...\n");
	if (serial_write(port, (uint8_t *)"R", 1) < 0)
		return -1;

	if (phone->chip_id == DB2010_2) {
		if (phone->break_rsa == 1) {
			uint8_t resp;
			int rcv_len = serial_wait_packet(port, &resp, 1, 10 * TIMEOUT);
			if (rcv_len <= 0)
				return -1;
		} else if (phone->anycid == 1) {
			uint8_t two3E[2];
			int rcv_len = serial_wait_packet(port, two3E, sizeof(two3E), 10 * TIMEOUT);
			if (rcv_len <= 0)
				return -1;

			if (serial_write(port, (uint8_t *)"R", 1) < 0)
				return -1;
		}
	}

	if (phone->chip_id == DB2020) {
		uint8_t three3E[3];
		int rcv_len = serial_wait_packet(port, three3E, sizeof(three3E), 10 * TIMEOUT);
		if (rcv_len <= 0)
			return -1;
	}

	uint8_t hello_buf[64];
	rcv_len = serial_wait_packet(port, hello_buf, sizeof(hello_buf), 10 * TIMEOUT);
	if (rcv_len <= 0)
		return -1;

	struct packetdata_t repl;
	if (cmd_decode_packet(hello_buf, rcv_len, &repl) != 0)
		return -1;

	loader_get_hello(&repl);

	// Activate loader
	if (loader_activate_payload(port, phone) != 0)
		return -1;

	phone->qhldr_sent = 1;

	return 0;

exit_error:
	free(buffer);
	return -1;
}

int loader_send_qhldr_noact(struct sp_port *port, const char *fname)
{
	char loader_name[512];
	snprintf(loader_name, sizeof(loader_name), "./loader/%s", fname);

	size_t fsize;
	uint8_t *buffer = load_file(loader_name, &fsize);
	if (!buffer) {
		fprintf(stderr, "can't read %s\n", loader_name);
		return -1;
	}

	struct babehdr_t *babehdr = (struct babehdr_t *)buffer;

	size_t qh_size = sizeof(struct babehdr_t);
	size_t qa_size = babehdr->prologuesize1;
	size_t qd_size = babehdr->payloadsize1;

	uint8_t *qh00 = buffer;
	uint8_t *qa00 = buffer + qh_size;
	uint8_t *qd00 = buffer + qh_size + qa_size;

	// --- Send QH00
	// printf("Send header ...\n");
	if (serial_write(port, (uint8_t *)"QH00", 4) < 0)
		goto exit_error;
	if (serial_wait_answer(port, "EsB", 5 * TIMEOUT, 0) < 0)
		goto exit_error;
	if (serial_write(port, qh00, qh_size) < 0)
		goto exit_error;
	if (serial_wait_answer(port, "EhM", 5 * TIMEOUT, 0) < 0)
		goto exit_error;

	// --- Send QA00
	// printf("Send prologue ...\n");
	if (serial_write(port, (uint8_t *)"QA00", 4) < 0)
		goto exit_error;
	if (serial_write_chunks(port, qa00, qa_size, 0x400) < 0)
		goto exit_error;
	if (serial_wait_answer(port, "EaT", 5 * TIMEOUT, 0) < 0)
		goto exit_error;
	if (serial_wait_answer(port, "EbS", 5 * TIMEOUT, 0) < 0)
		goto exit_error;

	// --- Send QD00
	// printf("Send body ...\n");
	if (serial_write(port, (uint8_t *)"QD00", 4) < 0)
		goto exit_error;
	if (serial_write_chunks(port, qd00, qd_size, 0x400) < 0)
		goto exit_error;
	if (serial_wait_answer(port, "EdQ", 5 * TIMEOUT, 0) < 0)
		goto exit_error;

	// --- Read hello response from loader =)
	uint8_t hello_buf[64];
	int rcv_len = serial_wait_packet(port, hello_buf, sizeof(hello_buf), 5 * TIMEOUT);
	if (rcv_len <= 0)
		goto exit_error;

	struct packetdata_t repl;
	if (cmd_decode_packet(hello_buf, rcv_len, &repl) != 0)
		goto exit_error;

	loader_get_hello(&repl);

	free(buffer);
	return 0;

exit_error:
	free(buffer);
	return -1;
}

int loader_send_qhldr(struct sp_port *port, struct phone_info *phone, const char *loader_name)
{
	if (phone->qhldr_sent == 1)
		return 0;

	if (loader_send_qhldr_noact(port, loader_name) != 0)
		return -1;

	if (loader_activate_payload(port, phone) != 0)
		return -1;

	phone->qhldr_sent = 1;

	printf("FLASH ID: 0x%x (%s)\n", phone->flash_id, get_flash_vendor(phone->flash_id));

	printf("OTP: LOCKED:%d CID:%d PAF:%d IMEI:%s\n", phone->otp_locked, phone->otp_cid, phone->otp_paf,
	       phone->otp_imei);

	if (phone->chip_id == DB2020 && phone->anycid == 0) {
		if (loader_get_erom_info(port, phone) != 0)
			return -1;

		printf("ACTIVE CID:%02d COLOR:%s\n", phone->erom_cid, color_get_name(phone->erom_color));
	}

	return 0;
}

int loader_send_encoded_cmd_and_data(struct sp_port *port, uint8_t cmd, uint8_t *data, size_t total_bytes)
{
	uint8_t buf[0x800];
	size_t sent_bytes = 0;
	int do_once = 1;

	serial_send_ack(port);

	while (sent_bytes < total_bytes || do_once) {
		int num = 0;
		uint8_t checksum = 0;
		do_once = 0;

		buf[num++] = SERIAL_HDR89; // hdr
		buf[num++] = cmd; // command

		int bytes_to_send = total_bytes - sent_bytes;
		int max_bytes = 0x7FF;
		if (bytes_to_send > max_bytes)
			bytes_to_send = max_bytes;

		// length = payload + continuebit
		buf[num++] = (bytes_to_send + 1) & 0xFF;
		buf[num++] = ((bytes_to_send + 1) >> 8) & 0xFF;

		// continuebit: 01 = more data coming, 00 = no more data coming
		uint8_t continuebit = (sent_bytes + bytes_to_send < total_bytes) ? 0x01 : 0x00;
		buf[num++] = continuebit;

		memcpy(&buf[num], &data[sent_bytes], bytes_to_send);
		num += bytes_to_send;

		// checksum
		for (int i = 0; i < num; i++)
			checksum ^= buf[i];
		checksum = (checksum + 7) & 0xFF;
		buf[num++] = checksum;

		// send data
		if (serial_write_chunks(port, buf, num, 0x400) < 0)
			return -1;

		// --- Read response
		if (continuebit) {
			if (serial_wait_ack(port, 10 * TIMEOUT) != 0)
				return -1;
		}

		sent_bytes += bytes_to_send;
	}

	return 0;
}

int loader_send_binary_noact(struct sp_port *port, const char *fname)
{
	char loader_name[512];
	snprintf(loader_name, sizeof(loader_name), "./loader/%s", fname);

	size_t fsize;
	uint8_t *buffer = load_file(loader_name, &fsize);
	if (!buffer) {
		fprintf(stderr, "can't read %s\n", loader_name);
		return -1;
	}

	struct babehdr_t *babehdr = (struct babehdr_t *)buffer;

	size_t qh_size = sizeof(struct babehdr_t);
	size_t qa_size = babehdr->prologuesize1;
	size_t qd_size = babehdr->payloadsize1;

	uint8_t *qh00 = buffer;
	uint8_t *qa00 = buffer + qh_size;
	uint8_t *qd00 = buffer + qh_size + qa_size;

	uint8_t resp[7] = {0};
	int rcv_len;

	struct packetdata_t repl;

	// printf("Send header ...\n");
	if (loader_send_encoded_cmd_and_data(port, 0x3C, qh00, qh_size) < 0)
		goto exit_error;
	rcv_len = serial_wait_packet(port, resp, sizeof(resp), 3 * TIMEOUT);
	if (rcv_len <= 0)
		goto exit_error;

	if (cmd_decode_packet(resp, rcv_len, &repl) != 0)
		return -1;

	if (repl.cmd != 0x3D) {
		fprintf(stderr, "Bad answer %02X\n", repl.cmd);
		return -1;
	}

	// printf("Send prologue ...\n");
	if (loader_send_encoded_cmd_and_data(port, 0x3C, qa00, qa_size) < 0)
		goto exit_error;

	rcv_len = serial_wait_packet(port, resp, sizeof(resp), 3 * TIMEOUT);
	if (rcv_len <= 0)
		goto exit_error;

	if (cmd_decode_packet(resp, rcv_len, &repl) != 0)
		return -1;

	if (repl.cmd != 0x3D) {
		fprintf(stderr, "Bad answer %02X\n", repl.cmd);
		return -1;
	}

	// printf("Send body ...\n");
	if (loader_send_encoded_cmd_and_data(port, 0x3C, qd00, qd_size) < 0)
		goto exit_error;
	rcv_len = serial_wait_packet(port, resp, sizeof(resp),
	                             20 * TIMEOUT); // PNX5230 need extra longer ;(
	if (rcv_len <= 0)
		goto exit_error;

	if (cmd_decode_packet(resp, rcv_len, &repl) != 0)
		return -1;

	if (repl.cmd != 0x3D) {
		fprintf(stderr, "Bad answer %02X\n", repl.cmd);
		return -1;
	}

	// Send ACK to start binary loader
	serial_send_ack(port);

	// --- Read hello response from loader
	uint8_t hello_buf[128];
	rcv_len = serial_read(port, hello_buf, sizeof(hello_buf), 10 * TIMEOUT);
	if (rcv_len <= 0)
		goto exit_error;

	if (cmd_decode_packet(hello_buf, rcv_len, &repl) != 0)
		return -1;

	loader_get_hello(&repl);

	free(buffer);
	return 0;

exit_error:
	free(buffer);
	return -1;
}

int loader_send_binary(struct sp_port *port, struct phone_info *phone, const char *loader_name)
{
	if (loader_send_binary_noact(port, loader_name) != 0)
		return -1;

	if (loader_activate_payload(port, phone) != 0)
		return -1;

	return 0;
}

int loader_get_erom_info(struct sp_port *port, struct phone_info *phone)
{
	uint8_t cmd_buf[32];
	int cmd_len = cmd_encode_binary_packet(CMD_READ_EROM_INFO, NULL, 0, cmd_buf);
	if (cmd_len <= 0)
		return -1;

	// --- send command
	if (serial_send_packetdata_ack(port, cmd_buf, cmd_len) < 0)
		return -1;

	uint8_t resp[0x13];
	int rcv_len = serial_read(port, resp, sizeof(resp), 5 * TIMEOUT);
	if (rcv_len <= 0)
		return -1;

	struct packetdata_t repl;
	if (cmd_decode_packet(resp, rcv_len, &repl) != 0)
		return -1;

	if (repl.data[1] & 1)
		phone->erom_color = BLUE;
	else if (repl.data[1] & 2)
		phone->erom_color = BROWN;
	else if (repl.data[1] & 4)
		phone->erom_color = RED;
	else if (repl.data[1] & 8)
		phone->erom_color = BLACK;
	else {
		phone->erom_color = BLACK;
		// return -1;
	}

	phone->erom_cid = repl.data[9];

	return 0;
}

int loader_get_otp_info(struct sp_port *port, struct phone_info *phone)
{
	uint8_t cmd_buf[32];
	int cmd_len = cmd_encode_binary_packet(CMD_READ_OTP_IMEI, NULL, 0, cmd_buf);
	if (cmd_len <= 0)
		return -1;

	// --- send command
	if (serial_send_packetdata_ack(port, cmd_buf, cmd_len) < 0)
		return -1;

	uint8_t resp[0x20];
	int rcv_len = serial_read(port, resp, sizeof(resp), 5 * TIMEOUT);
	if (rcv_len <= 0)
		return -1;

	struct packetdata_t repl;
	if (cmd_decode_packet(resp, rcv_len, &repl) != 0)
		return -1;

	phone->otp_status = repl.data[0];
	phone->otp_locked = repl.data[1];
	phone->otp_cid = (repl.data[3] << 8) | repl.data[2];
	phone->otp_paf = repl.data[4];
	memcpy(phone->otp_imei, repl.data + 5, 14);
	phone->otp_imei[14] = '\0';

	if (strncmp(phone->otp_imei, "353456", 6) == 0 || strncmp(phone->otp_imei, "353457", 6) == 0) {
		phone->is_z1010 = 1;
	}

	return 0;
}

int loader_get_flash_info(struct sp_port *port, struct phone_info *phone)
{
	uint8_t cmd_buf[32];
	int cmd_len = cmd_encode_binary_packet(CMD_GET_FLASHID, NULL, 0, cmd_buf);
	if (cmd_len <= 0)
		return -1;

	// --- send command
	if (serial_send_packetdata_ack(port, cmd_buf, cmd_len) < 0)
		return -1;

	uint8_t resp[8];
	int rcv_len = serial_wait_packet(port, resp, sizeof(resp), 3 * TIMEOUT);
	if (rcv_len <= 0)
		return -1;

	struct packetdata_t repl;
	if (cmd_decode_packet(resp, rcv_len, &repl) != 0)
		return -1;

	if (repl.cmd != 0x0A && repl.length != 2)
		return -1;

	phone->flash_id = (repl.data[0] << 8) | repl.data[1];

	switch (phone->flash_id) {
	case 0x8964:
		phone->flashblocksize = 0x10000;
		break;
	case 0x200D:
	case 0x890D:
		phone->flashblocksize = 0x20000;
		break;
	case 0x2019:
	case 0x897E:
		phone->flashblocksize = 0x40000;
		break;
	default:
		printf("unknown flash chip 0x%X\n", phone->flash_id);
		phone->flashblocksize = 0;
		break;
	}

	return 0;
}

int loader_activate_gdfs(struct sp_port *port)
{
	printf("Activating GDFS.. ");

	uint8_t cmd_buf[8];
	int cmd_len = cmd_encode_binary_packet(CMD_ACTIVATE_GDFS, NULL, 0, cmd_buf);
	if (cmd_len <= 0)
		return -1;

	// --- send command
	if (serial_send_packetdata_ack(port, cmd_buf, cmd_len) < 0)
		return -1;

	// Wait for reply
	uint8_t resp[7];
	int rcv_len = serial_wait_packet(port, resp, sizeof(resp), 20 * TIMEOUT);
	if (rcv_len <= 0)
		return -1;

	struct packetdata_t repl;
	if (cmd_decode_packet(resp, rcv_len, &repl) != 0)
		return -1;

	if (repl.cmd != 0x1D && repl.data[0] != 0x00) {
		printf("failed\n");
		return -1;
	}

	printf("activated\n");
	return 0;
}

int loader_shutdown(struct sp_port *port)
{
	printf("Shutdown phone\n");

	if (loader_type == LDR_CHIPSELECT)
		goto exit_done;

	uint8_t cmd_buf[8];
	int cmd_len = cmd_encode_binary_packet(CMD_SHUTDOWN, NULL, 0, cmd_buf);
	if (cmd_len <= 0)
		return -1;

	if (serial_send_packetdata_ack(port, cmd_buf, cmd_len) < 0)
		return -1;

	if (serial_wait_ack(port, 10 * TIMEOUT) != 0)
		return -1;

exit_done:
	printf("Done");
	return 0;
}

// should activate flash_mode when success, only for identify action.
int loader_enter_flashmode(struct sp_port *port, struct phone_info *phone)
{
	switch (phone->chip_id) {
	case DB2000:
		if (phone->erom_cid == 49)
			return loader_send_qhldr(port, phone, DB2000_PILOADER_RED_CID03_P3B);
		else if (phone->erom_cid == 36)
			return loader_send_qhldr(port, phone, DB2000_PILOADER_RED_CID00_R3A);
		return loader_send_qhldr(port, phone, DB2000_PILOADER_RED_CID03_P3B);
	case DB2010_1:
	case DB2010_2:
		if (phone->erom_cid <= 36)
			return loader_send_qhldr(port, phone, DB2010_PILOADER_RED_CID00_P3L);
		return loader_send_qhldr(port, phone, DB2010_PILOADER_RED_CID00_P4D);
	case DB2020:
		return loader_send_qhldr(port, phone, DB2020_PILOADER_RED_CID01_P3M);
	default:
		fprintf(stderr, "[QHLDR] Unknown CHIP ID %X\n", phone->chip_id);
		return -1;
	}
}

// Return number of bytes received, or -1 on exit_error
int loader_send_packet_pnx(struct sp_port *port, uint8_t block, uint8_t msb, uint8_t lsb, uint8_t *resp,
                           size_t resp_max)
{
	uint8_t cmd_buf[7] = {'I', 'C', 'G', '1', block, lsb, msb};

	// send data
	if (serial_write(port, cmd_buf, sizeof(cmd_buf)) < 0)
		return -1;

	uint8_t hdr[3];
	int rcv_len = serial_read(port, hdr, sizeof(hdr), 10 * TIMEOUT);
	if (rcv_len <= 0)
		return -1;

	if (hdr[0] != block || hdr[1] != lsb || hdr[2] != msb)
		return -1;

	uint8_t len[4];
	rcv_len = serial_read(port, len, sizeof(len), 10 * TIMEOUT);
	if (rcv_len <= 0)
		return -1;

	int datasize = get_word(len);
	if (datasize > (int)resp_max)
		return -1;

	rcv_len = serial_read(port, resp, datasize, 10 * TIMEOUT);
	if (rcv_len != datasize)
		return -1;

	return datasize;
}
