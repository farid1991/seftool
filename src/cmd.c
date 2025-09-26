#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "cmd.h"

// --- cmd_encode_binary_packet ---
// cmd      = command ID
// data     = payload data
// datasize = payload size
// outbuf   = buffer to hold the generated command
// returns number of bytes written
int cmd_encode_binary_packet(uint8_t cmd, const uint8_t *data, int datasize, uint8_t *outbuf)
{
	int num = 0;
	uint8_t checksum = 0;

	// Packet
	outbuf[num++] = SERIAL_HDR89;
	outbuf[num++] = cmd;
	outbuf[num++] = datasize & 0xFF;
	outbuf[num++] = (datasize >> 8) & 0xFF;

	// Copy payload
	if (data && datasize > 0)
	{
		memcpy(&outbuf[num], data, datasize);
		num += datasize;
	}

	// Compute checksum over all header+payload bytes
	for (int i = 0; i < num; i++)
	{
		checksum ^= outbuf[i];
	}

	outbuf[num++] = (checksum + 7) & 0xFF;

	return num;
}

// --- cmd_encode_write_gdfs ---
int cmd_encode_write_gdfs(uint8_t block, uint8_t lsb, uint8_t msb,
						  const uint8_t *data, int datasize, uint8_t *outbuf)
{
	if (datasize < 0 || datasize > 0x1000)
		return -1;

	uint8_t payload[3 + 0x1000];

	payload[0] = block;
	payload[1] = lsb;
	payload[2] = msb;

	if (datasize > 0 && data)
		memcpy(&payload[3], data, datasize);

	return cmd_encode_binary_packet(0x20, payload, 3 + datasize, outbuf);
}

// --- cmd_encode_read_gdfs ---
int cmd_encode_read_gdfs(uint8_t block, uint8_t lsb, uint8_t msb, uint8_t *outbuf)
{
	uint8_t data[3];
	data[0] = block;
	data[1] = lsb;
	data[2] = msb;

	return cmd_encode_binary_packet(0x21, data, sizeof(data), outbuf);
}

// --- cmd_encode_csloader_packet ---
// cmd      = command ID
// subcmd   = first byte of payload
// csdata   = payload data
// datasize = size of payload data
// outbuf   = buffer to hold full command
int cmd_encode_csloader_packet(uint8_t cmd, uint8_t subcmd,
							   const uint8_t *csdata, int datasize,
							   uint8_t *outbuf)
{
	int num = 0;
	uint8_t checksum = 0;

	// Header
	outbuf[num++] = SERIAL_HDR89;
	outbuf[num++] = cmd;
	outbuf[num++] = (datasize + 1) & 0xFF; // payload size = subcmd + data
	outbuf[num++] = ((datasize + 1) >> 8) & 0xFF;

	// Subcommand
	outbuf[num++] = subcmd;

	// Payload
	if (csdata && datasize > 0)
	{
		memcpy(&outbuf[num], csdata, datasize);
		num += datasize;
	}

	// Checksum (header + payload)
	for (int i = 0; i < num; i++)
	{
		checksum ^= outbuf[i];
	}
	outbuf[num++] = (checksum + 7) & 0xFF;

	return num;
}

int cmd_decode_packet_ack(const uint8_t *buf, int size, struct packetdata_t *out)
{
	if (size < 6)
	{
		fprintf(stderr, "[cmd_decode_packet_ack]Reply too short [Got %d byte]\n", size);
		return -1;
	}

	if (buf[0] != SERIAL_ACK)
	{
		fprintf(stderr, "[cmd_decode_packet_ack] Invalid reply\nExpected: 0x06 0x89: Got: 0x%02X 0x%02X\n", buf[0], buf[1]);
		return -1;
	}

	out->ack = buf[0]; // must be 0x06
	out->hdr = buf[1]; // must be 0x89
	out->cmd = buf[2]; // command ID
	out->length = buf[3] | (buf[4] << 8);

	if (size < 5 + out->length + 1)
	{
		fprintf(stderr, "Reply length mismatch: expected %d got %d\n",
				out->length, size - 5);
		return -1;
	}

	memcpy(out->data, buf + 5, out->length);
	out->checksum = buf[5 + out->length];

	// Verify checksum
	uint8_t sum = 0;
	for (int i = 1; i < 5 + out->length; i++) // skip ack at buf[0]
	{
		sum ^= buf[i];
	}
	sum = (sum + 7) & 0xFF;

	if (sum != out->checksum)
	{
		fprintf(stderr, "Checksum mismatch: got 0x%02X expected 0x%02X\n",
				out->checksum, sum);
		return -1;
	}

	return 0;
}

int cmd_decode_packet_noack(const uint8_t *buf, int size, struct packetdata_t *out)
{
	if (size < 5)
	{
		fprintf(stderr, "Reply too short\n");
		return -1;
	}

	if (buf[0] != SERIAL_HDR89)
	{
		fprintf(stderr, "[cmd_decode_packet_noack] Invalid header: 0x%02X\n", buf[0]);
		return -1;
	}

	out->ack = 0x00;   // no ack present
	out->hdr = buf[0]; // 0x89
	out->cmd = buf[1]; // command ID
	out->length = buf[2] | (buf[3] << 8);

	if (size < 4 + out->length + 1)
	{
		fprintf(stderr, "Reply length mismatch: expected %d got %d\n",
				out->length, size - 4);
		return -1;
	}

	memcpy(out->data, buf + 4, out->length);
	out->checksum = buf[4 + out->length];

	// --- Verify checksum ---
	uint8_t sum = 0;
	for (int i = 0; i < 4 + out->length; i++) // include marker, cmd, len, data
	{
		sum ^= buf[i];
	}
	sum = (sum + 7) & 0xFF;

	if (sum != out->checksum)
	{
		fprintf(stderr, "Checksum mismatch: got 0x%02X expected 0x%02X\n",
				out->checksum, sum);
		return -1;
	}

	return 0;
}

int cmd_decode_packet(const uint8_t *buf, int size, struct packetdata_t *out)
{
	if (buf[0] == SERIAL_ACK && buf[1] == SERIAL_HDR89) // 06 89 XX XX XX XX
	{
		return cmd_decode_packet_ack(buf, size, out);
	}
	else if (buf[0] == SERIAL_HDR89) // 89 XX XX XX XX
	{
		return cmd_decode_packet_noack(buf, size, out);
	}
	else if (buf[0] == 0 && buf[1] == SERIAL_HDR89) // some CSLOADER
	{
		return cmd_decode_packet_noack(buf + 1, size - 1, out);
	}
	else if (buf[0] == 0x3E && buf[1] == SERIAL_HDR89) // DB2010_RESPIN_PRODLOADER_SETOOL2 & DB2020_PRELOADER_FOR_SETOOL2
	{
		return cmd_decode_packet_noack(buf + 1, size - 1, out);
	}
	else
	{
		fprintf(stderr, "Expected:89 XX XX XX XX || Got: %02X %02X\n", buf[0], buf[1]);
		return -1;
	}
}