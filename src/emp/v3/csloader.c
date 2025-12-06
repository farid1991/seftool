#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <libserialport.h>

#ifdef _WIN32
#include <direct.h> // _mkdir
#define MKDIR(path) _mkdir(path)
#define RMDIR(path) _rmdir(path)
#define PATHSEP '\\'
#else
#include <sys/stat.h>
#include <unistd.h>
#define MKDIR(path) mkdir(path, 0755)
#define RMDIR(path) rmdir(path)
#define PATHSEP '/'
#endif

#include <core/common.h>
#include <core/serial.h>
#include <emp/common/cmd.h>
#include <utils/scandir.h>
#include <utils/zipper.h>

#include "loader.h"
#include "csloader.h"
#include "gdfs.h"

#define GDFS_DATA_OFFSET 2
int csloader_read_gdfs_var(struct sp_port *port, uint8_t block, uint16_t unit, uint8_t *outbuf, size_t outsize)
{
	uint8_t cmd_buf[8];
	uint8_t resp[2048];

	uint8_t gdfsvar[3] = {block, unit & 0xFF, (unit >> 8) & 0xFF};

	int cmd_len = cmd_encode_csloader_packet(CMD_CS_GDFS, CMD_GDFS_READVAR, gdfsvar, sizeof(gdfsvar), cmd_buf);
	if (cmd_len <= 0)
		return -1;

	if (serial_send_packetdata_ack(port, cmd_buf, cmd_len) < 0)
		return -1;

	int rcv_len = serial_read(port, resp, sizeof(resp), 10 * TIMEOUT);
	if (rcv_len <= 0)
		return -1;

	struct packetdata_t repl;
	if (cmd_decode_packet(resp, rcv_len, &repl) != 0)
		return -1;

	if (repl.length < GDFS_DATA_OFFSET)
		return 0;

	size_t datalen = repl.length - GDFS_DATA_OFFSET;
	if (outbuf && outsize >= datalen) {
		memcpy(outbuf, repl.data + GDFS_DATA_OFFSET, datalen);
		return (int)datalen;
	}

	return -1;
}

int csloader_gdfs_get_phonename(struct sp_port *port, struct phone_info *phone)
{
	uint8_t block, lo, hi;

	switch (phone->chip_id) {
	case DB2000:
		block = phone->is_z1010 ? 0x04 : 0x02;
		lo = 0x8F;
		hi = 0x0C;
		break;

	case DB2010_1:
	case DB2010_2:
		block = 0x02;
		lo = 0x8F;
		hi = 0x0C;
		break;

	case DB2020:
	case PNX5230:
		block = 0x02;
		lo = 0xBB;
		hi = 0x0D;
		break;

	default:
		fprintf(stderr, "Unsupported platform for GDFS test.\n");
		return -1;
	}

	uint8_t gdfsbuf[256];
	int len = csloader_read_gdfs_var(port, block, (hi << 8) | lo, gdfsbuf, sizeof(gdfsbuf));

	if (len <= 0)
		return -1;

	// --- Convert UCS2 string to multibyte (ANSI)
	wcstombs(phone->phone_name, (wchar_t *)gdfsbuf, len);

	return 0;
}

int csloader_gdfs_get_cxc_article(struct sp_port *port, struct phone_info *phone)
{
	uint8_t block, lo, hi;

	switch (phone->chip_id) {
	case DB2010_1:
	case DB2010_2:
		block = 0x02;
		lo = 0xE9;
		hi = 0x0C;
		break;

	case DB2020:
	case PNX5230:
		block = 0x02;
		lo = 0x15;
		hi = 0x0E;
		break;

	default:
		return 0;
	}

	uint8_t gdfsbuf[256];
	int len = csloader_read_gdfs_var(port, block, (hi << 8) | lo, gdfsbuf, sizeof(gdfsbuf));
	if (len <= 0)
		return -1;

	// gdfsbuf contains ASCII like:
	// "R4EA031     prgCXC1250466_GENERIC_DO"
	// "R11AA011    prg1205-7395_CHINA_JH"

	const char *start = strstr((char *)gdfsbuf, "R"); // All SE firmware version start with 'R'
	const char *prg = strstr((char *)gdfsbuf, "prg");
	if (!start || !prg)
		return -1;

	// trim spaces before "prg"
	const char *p = prg;
	while (p > start && isspace((unsigned char)*(p - 1)))
		p--;

	char fw_ver[16] = {0};
	char fw_name[64] = {0};

	size_t len_ver = p - start;
	if (len_ver >= sizeof(fw_ver))
		len_ver = sizeof(fw_ver) - 1;
	strncpy(fw_ver, start, len_ver);
	fw_ver[len_ver] = '\0';

	// skip "prg"
	prg += 3;
	while (*prg && isspace((unsigned char)*prg))
		prg++;

	strncpy(fw_name, prg, sizeof(fw_name) - 1);

	snprintf(phone->cxc_article, sizeof(phone->cxc_article), "%s_%s", fw_ver, fw_name);

	return 0;
}

int csloader_gdfs_get_cxc_version(struct sp_port *port, struct phone_info *phone)
{
	uint8_t block, lo, hi;

	switch (phone->chip_id) {
	case DB2010_1:
	case DB2010_2:
		block = 0x02;
		lo = 0xEA;
		hi = 0x0C;
		break;

	case DB2020:
		block = 0x02;
		lo = 0x16;
		hi = 0x0E;
		break;

	default:
		fprintf(stderr, "Unsupported platform for GDFS CXC version.\n");
		return -1;
	}

	uint8_t gdfsbuf[64];
	int len = csloader_read_gdfs_var(port, block, (hi << 8) | lo, gdfsbuf, sizeof(gdfsbuf));
	if (len <= 0)
		return -1;

	strncpy(phone->cxc_version, (char *)gdfsbuf, sizeof(gdfsbuf));

	return 0;
}

int csloader_write_gdfs_var(struct sp_port *port, uint8_t block, uint8_t lo, uint8_t hi, uint8_t *data, uint32_t size)
{
	uint8_t *gdfs_var = malloc(size + 7);
	gdfs_var[0] = block;
	gdfs_var[1] = lo;
	gdfs_var[2] = hi;
	memcpy(&gdfs_var[3], &size, 4);
	memcpy(&gdfs_var[7], data, size);

	uint8_t cmd_buf[0x800];
	uint8_t resp[8];

	int cmd_len = cmd_encode_csloader_packet(CMD_CS_GDFS, CMD_GDFS_WRITEVAR, gdfs_var, size + 7, cmd_buf);
	free(gdfs_var);

	if (cmd_len <= 0)
		return -1;

	if (serial_send_packetdata_ack(port, cmd_buf, cmd_len) < 0)
		return -1;

	int rcv_len = serial_read(port, resp, sizeof(resp), TIMEOUT);
	if (rcv_len <= 0)
		return -1;

	struct packetdata_t repl;
	if (cmd_decode_packet(resp, rcv_len, &repl) != 0)
		return -1;

	if (repl.cmd != 0x04 && repl.data[1] != 0x00)
		return -1;

	return 0;
}

int csloader_write_gdfs(struct sp_port *port, const char *inputfname)
{
	printf("Restore GDFS...\n");
	size_t datasize;
	uint8_t *buf = load_file(inputfname, &datasize);
	if (!buf) {
		fprintf(stderr, "can't read %s\n", inputfname);
		return -1;
	}

	uint32_t readbytes = 0;
	uint8_t *bufpointer = &buf[4];

	datasize -= 4;
	uint32_t varcount = get_word(&buf[0]);
	printf("Attempting to write %i variables...\n", varcount);
	varcount = 0;
	while (readbytes < datasize) {
		uint8_t block = bufpointer[0];
		bufpointer++;
		uint8_t lo = bufpointer[0];
		bufpointer++;
		uint8_t hi = bufpointer[0];
		bufpointer++;
		uint32_t varsize = get_word(bufpointer);
		readbytes += varsize + 7;
		uint32_t realsize;
		if (varsize > 0x600)
			realsize = 0x600;
		else
			realsize = varsize;
		bufpointer += 4;
		printf("\rWriting %.04d bytes to block 0x%.02x, unit 0x%02X%02X", varsize, block, hi, lo);

		if (csloader_write_gdfs_var(port, block, lo, hi, bufpointer, realsize) != 0) {
			free(buf);
			printf("\n\nWrote %i units!\n", varcount);
			printf("GDFS was not fully restored!\n");
			return -1;
		}
		bufpointer += varsize;
		varcount++;
	}
	free(buf);
	printf("\n\nWrote %i variables!\n", varcount);
	printf("GDFS was restored successfully!\n");

	return 0;
}

int csloader_read_gdfs(struct sp_port *port, struct phone_info *phone)
{
	printf("Back up GDFS...\n");

	uint8_t cmd_buf[8];
	uint8_t resp[0x10000] = {0};

	int cmd_len = cmd_encode_csloader_packet(CMD_CS_GDFS, CMD_GDFS_READALL, NULL, 0, cmd_buf);
	if (cmd_len <= 0)
		return -1;

	if (serial_send_packetdata_ack(port, cmd_buf, cmd_len) < 0)
		return -1;

	if (serial_wait_ack(port, 100 * TIMEOUT) < 0)
		return -1;

	int rcv_len = serial_read(port, &resp[0], 10, 500 * TIMEOUT);
	if (rcv_len <= 0)
		return -1;

	uint32_t datasize = (resp[2] | (resp[3] << 8)) + 1;
	uint32_t varcount = get_word(&resp[6]);

	printf("stated number of vars: %d\n", varcount);

	char outfile[512];
	snprintf(outfile, sizeof(outfile), "./backup/GDFS_%s_%s.bin", phone->phone_name, phone->otp_imei);

	FILE *out = fopen(outfile, "wb");
	if (!out) {
		fprintf(stderr, "Can not open backup file\n");
		return -1;
	}

	uint32_t bytesread = 10;
	uint32_t chunkcount = 0;
	for (uint32_t i = 0; i < varcount; i++) {
		serial_read(port, &resp[bytesread], 1, 100 * TIMEOUT);

		if (bytesread >= datasize) {
			printf("Found: %d units\n", chunkcount);
			fwrite(&resp[6], sizeof(uint8_t), bytesread - 6, out);
			serial_send_ack(port);
			serial_read(port, resp, 6, 50 * TIMEOUT);
			datasize = (resp[2] | (resp[3] << 8)) + 1;
			bytesread = 6;
			chunkcount = 0;
			serial_read(port, &resp[bytesread], 1, 100 * TIMEOUT);
		}
		bytesread++;
		serial_read(port, &resp[bytesread], 6, 100 * TIMEOUT);
		bytesread += 2;
		uint32_t varsize = get_word(&resp[bytesread]);
		bytesread += 4;
		serial_read(port, &resp[bytesread], varsize, 100 * TIMEOUT);

		bytesread += varsize;
		chunkcount++;
	}
	printf("Found: %d units\n\n", chunkcount);
	fwrite(&resp[6], sizeof(char), bytesread - 6, out);
	fclose(out);

	printf("GDFS saved %s\n", outfile);
	printf("GDFS backup successfully!\n");

	// read the byte left on the phone queue
	uint8_t byteleft;
	serial_read(port, &byteleft, 1, 50 * TIMEOUT);

	return 0;
}

int csloader_parse_gdfs_script(struct sp_port *port, const char *inputfname, const char *outputfname)
{
	printf("\nRun GDFS-script... %s\n", inputfname);

	FILE *f = fopen(inputfname, "r");
	if (!f) {
		fprintf(stderr, "GDFS-Script: Error (Couldn't open input file!)\n");
		return -1;
	}

	FILE *fout = fopen(outputfname, "w");
	if (!fout) {
		fprintf(stderr, "GDFS-Script: Error (Couldn't open output file!)\n");
		fclose(f);
		return -1;
	}

	// --- Write header at top ---
	time_t now = time(NULL);
	char timestr[64];
	strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", localtime(&now));
	fprintf(fout, "; Created with seftool\n; Creation time and date: %s\n\n", timestr);

	char linebuf[4096];
	uint32_t varcount = 0, varreadcount = 0;

	while (fgets(linebuf, sizeof(linebuf), f)) {
		// skip empty or comment lines
		if (linebuf[0] == '\0' || linebuf[0] == '\n' || linebuf[0] == '#' || linebuf[0] == ';')
			continue;

		if (strncmp(linebuf, "gdfswrite:", 10) == 0) {
			uint32_t block, hi, lo;
			char datahex[0x680 * 2 + 1] = {0};

			int n = sscanf(linebuf + 10, "%4x%2x%2x%[0-9A-Fa-f]", &block, &hi, &lo, datahex);
			if (n >= 3) {
				uint8_t vardata[0x680];
				size_t datalen = 0;

				for (char *p = datahex; *p && *(p + 1); p += 2) {
					unsigned byte;
					if (sscanf(p, "%2x", &byte) != 1)
						break;
					vardata[datalen++] = (uint8_t)byte;
					if (datalen >= sizeof(vardata))
						break;
				}

				printf("Writing GDFS var %.04X/%02X%02X\n", block, hi, lo);
				csloader_write_gdfs_var(port, block, lo, hi, vardata, datalen);
				varcount++;
			}
		} else if (strncmp(linebuf, "gdfsread:", 9) == 0) {
			uint32_t block, hi, lo;
			if (sscanf(linebuf + 9, "%4x%2x%2x", &block, &hi, &lo) == 3) {
				uint8_t vardata[0x10000];
				memset(vardata, 0, sizeof(vardata));

				uint16_t unit = (hi << 8) | lo;
				int len = csloader_read_gdfs_var(port, (uint8_t)block, unit, vardata, sizeof(vardata));

				if (len > 0) {
					printf("Reading GDFS var %.04X/%02X%02X\n", block, hi, lo);
					fprintf(fout, "gdfswrite:%04X%02X%02X", block, hi, lo);
					for (int i = 0; i < len; i++)
						fprintf(fout, "%02X", vardata[i]);
					fprintf(fout, "\n");

					varreadcount++;
				} else {
					fprintf(stderr, "Failed to read GDFS var %.04X/%02X%02X\n", block, hi, lo);
				}
			}
		} else {
			fprintf(stderr, "Warning: Unknown or unsupported script line skipped: %s", linebuf);
		}
	}

	fclose(f);
	fclose(fout);

	printf("Wrote %u variables!\n", varcount);
	printf("Read %u variables to %s!\n", varreadcount, outputfname);
	if (varreadcount == 0)
		remove(outputfname);

	printf("GDFS-Script was run successfully!\n");
	return 0;
}

// FSX Operation

int csloader_start_fsx_server(struct sp_port *port)
{
	printf("Starting FSX server...");
	uint8_t cmd_buf[8];
	int cmd_len = cmd_encode_csloader_packet(CMD_CS_FSX, CMD_FSX_START, NULL, 0, cmd_buf);
	if (cmd_len <= 0)
		return -1;

	// --- send command
	if (serial_send_packetdata_ack(port, cmd_buf, cmd_len) < 0)
		return -1;

	uint8_t resp[8];
	int rcv_len = serial_wait_packet(port, resp, sizeof(resp), 1000 * TIMEOUT);
	if (rcv_len <= 0)
		return -1;

	struct packetdata_t repl = {0};
	if (cmd_decode_packet(resp, rcv_len, &repl) != 0)
		return -1;

	if (repl.cmd != CMD_CS_FSX) {
		printf("failed\n");
		return -1;
	}

	printf("started\n");
	return 0;
}

int csloader_get_working_directory(struct sp_port *port, char *cwd, size_t max_len)
{
	if (!cwd || max_len == 0)
		return -1;

	uint8_t cmd_buf[16];
	struct packetdata_t repl = {0};

	// build GETDIR command
	int cmd_len = cmd_encode_csloader_packet(CMD_CS_FSX, CMD_FSX_GETDIR, NULL, 0, cmd_buf);
	if (cmd_len <= 0)
		return -1;

	if (serial_send_packetdata_ack(port, cmd_buf, cmd_len) < 0)
		return -1;

	if (serial_wait_ack(port, 10 * TIMEOUT) != 0)
		return -1;

	uint8_t resp[256];
	int rcv_len = serial_read(port, resp, sizeof(resp), 3 * TIMEOUT);
	if (rcv_len <= 0)
		return -1;

	if (cmd_decode_packet(resp, rcv_len, &repl) != 0)
		return -1;

	if (repl.cmd != CMD_CS_FSX || repl.length < 4)
		return -1;

	// repl.data[0..1] = status code
	if (repl.data[1] != 0)
		return -1;

	// repl.data[2...] = UCS2 string (null-terminated)
	size_t cwd_len;
	ucs2_to_ascii(repl.data, repl.length, GDFS_DATA_OFFSET, cwd, max_len, &cwd_len);

	return cwd_len;
}

int csloader_change_directory(struct sp_port *port, const char *dirname)
{
	// printf("cd %s\n", dirname);

	uint8_t data[0x100];
	size_t size = 0;

	size += ascii_to_ucs2(dirname, &data[size]);

	uint8_t cmd_buf[0x100 + 7];
	int cmd_len = cmd_encode_csloader_packet(CMD_CS_FSX, CMD_FSX_CHDIR, data, size, cmd_buf);
	if (cmd_len <= 0)
		return -1;

	if (serial_send_packetdata_ack(port, cmd_buf, cmd_len) < 0)
		return -1;

	uint8_t resp[32];
	struct packetdata_t repl = {0};
	int rcv_len = serial_read(port, resp, sizeof(resp), 3 * TIMEOUT);
	if (rcv_len <= 0)
		return -1;
	if (cmd_decode_packet(resp, rcv_len, &repl) != 0)
		return -1;

	if (repl.cmd != CMD_CS_FSX || repl.length < 2)
		return -1;
	if (repl.data[1] != 0)
		return -1;

	return 0;
}

int csloader_stat(struct sp_port *port, const char *filename, ose_stat_t *st)
{
	if (!filename || !st)
		return -1;

	uint8_t cmd_buf[0x100];
	struct packetdata_t repl;

	uint8_t data[0x256];
	size_t size = 0;
	size += ascii_to_ucs2(filename, &data[size]);

	int cmd_len = cmd_encode_csloader_packet(CMD_CS_FSX, CMD_FSX_STAT, data, size, cmd_buf);
	if (cmd_len <= 0)
		return -1;

	if (serial_send_packetdata_ack(port, cmd_buf, cmd_len) < 0)
		return -1;

	uint8_t resp[0x20];
	int rcv_len = serial_read(port, resp, sizeof(resp), 3 * TIMEOUT);
	if (rcv_len <= 0)
		return -1;
	if (cmd_decode_packet(resp, rcv_len, &repl) != 0)
		return -1;
	if (repl.length < sizeof(ose_stat_t) || repl.data[1] != 0)
		return -1;

	// --- Copy into ose_stat_t ---
	memcpy(st, &repl.data[2], sizeof(ose_stat_t));

	uint16_t attr = st->st_mode; // swap
	st->st_mode = 0;

	// --- Base permissions ---
	if (attr & 0x0001)
		st->st_mode |= OSEATTR_EXECUTABLE;
	if (attr & 0x0002)
		st->st_mode |= OSEATTR_WRITEABLE;
	if (attr & 0x0004)
		st->st_mode |= OSEATTR_READABLE;

	// --- Owner / group bits if they exist ---
	if (attr & 0x0010)
		st->st_mode |= OSEATTR_OWNER_WRITEABLE;
	if (attr & 0x0020)
		st->st_mode |= OSEATTR_OWNER_READABLE;

	// --- Directory flag ---
	if (attr & 0x4000)
		st->st_mode |= OSEATTR_DIRECTORY;

	return 0;
}

// --- Helper macro ---
#define ADD_ENTRY(src_name, d_type_flag)                                                                               \
	do {                                                                                                           \
		if (count >= capacity) {                                                                               \
			capacity = capacity ? capacity * 2 : 16;                                                       \
			cs_entry_t *tmp = realloc(list, capacity * sizeof(cs_entry_t));                                \
			if (!tmp) {                                                                                    \
				free(list);                                                                            \
				return -1;                                                                             \
			}                                                                                              \
			list = tmp;                                                                                    \
		}                                                                                                      \
		strncpy(list[count].name, src_name, sizeof(list[count].name) - 1);                                     \
		list[count].name[sizeof(list[count].name) - 1] = '\0';                                                 \
		list[count].d_type = d_type_flag;                                                                      \
		count++;                                                                                               \
	} while (0)

int csloader_list(struct sp_port *port, cs_entry_t **out_list, size_t *out_count)
{
	if (!out_list || !out_count)
		return -1;

	uint8_t resp[0x1000] = {0};
	struct packetdata_t repl;
	size_t pos;

	cs_entry_t *list = NULL;
	size_t count = 0;
	size_t capacity = 0;

	// --- Directories ---
	uint8_t cmd_buf[16];
	int cmd_len = cmd_encode_csloader_packet(CMD_CS_FSX, CMD_FSX_LISTDIRS, NULL, 0, cmd_buf);
	if (cmd_len <= 0)
		goto fail;
	if (serial_send_packetdata_ack(port, cmd_buf, cmd_len) < 0)
		goto fail;

	int rcv_len = serial_read(port, resp, sizeof(resp), 10 * TIMEOUT);
	if (rcv_len <= 0)
		goto fail;
	if (cmd_decode_packet(resp, rcv_len, &repl) != 0)
		goto fail;
	if (repl.length < 4 || repl.data[1] != 0)
		goto fail;

	pos = 2;
	uint16_t numdirs = *(uint16_t *)&repl.data[pos];
	pos += 2;

	for (int i = 0; i < numdirs; i++) {
		char dirname[256];
		size_t len;
		if (ucs2_to_ascii(repl.data, repl.length, pos, dirname, sizeof(dirname), &len) != 0)
			goto fail;

		ADD_ENTRY(dirname, 1); // Directory
		pos += len;
	}

	// --- Files ---
	cmd_len = cmd_encode_csloader_packet(CMD_CS_FSX, CMD_FSX_LISTFILES, NULL, 0, cmd_buf);
	if (cmd_len <= 0)
		goto fail;
	if (serial_send_packetdata_ack(port, cmd_buf, cmd_len) < 0)
		goto fail;

	rcv_len = serial_read(port, resp, sizeof(resp), 10 * TIMEOUT);
	if (rcv_len <= 0)
		goto fail;
	if (cmd_decode_packet(resp, rcv_len, &repl) != 0)
		goto fail;
	if (repl.length < 4 || repl.data[1] != 0)
		goto fail;

	pos = 2;
	uint16_t numfiles = *(uint16_t *)&repl.data[pos];
	pos += 2;

	for (int i = 0; i < numfiles; i++) {
		char filename[256];
		size_t len;
		if (ucs2_to_ascii(repl.data, repl.length, pos, filename, sizeof(filename), &len) != 0)
			goto fail;

		ADD_ENTRY(filename, 0); // File
		pos += len;
	}

	*out_list = list;
	*out_count = count;
	return 0;

fail:
	free(list);
	return -1;
}

int csloader_make_directory(struct sp_port *port, const char *dirname)
{
	if (!dirname) {
		fprintf(stderr, "Invalid argument to csloader_make_directory()\n");
		return -1;
	}

	// printf("mkdir %s\n", dirname);

	uint8_t data[0x100];
	size_t size = 0;

	// UCS2 dirname
	size += ascii_to_ucs2(dirname, &data[size]);

	// Attributes (DWORD, little endian)
	uint32_t ose_attr = OSEATTR_WRITEABLE | OSEATTR_READABLE;
	data[size++] = (uint8_t)(ose_attr & 0xFF);
	data[size++] = (uint8_t)((ose_attr >> 8) & 0xFF);
	data[size++] = (uint8_t)((ose_attr >> 16) & 0xFF);
	data[size++] = (uint8_t)((ose_attr >> 24) & 0xFF);

	uint8_t cmd_buf[0x100 + 7] = {0};
	int cmd_len = cmd_encode_csloader_packet(CMD_CS_FSX, CMD_FSX_MKDIR, data, size, cmd_buf);
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

	if (repl.cmd != CMD_CS_FSX) {
		fprintf(stderr, "Bad reply %02X expected %02X\n", repl.cmd, CMD_CS_FSX);
		return -1;
	}

	// if (repl.data[1] == 0)
	//     printf("OK\n");
	// else if (repl.data[1] == 0x6)
	//     printf("dir already exist\n");
	// else
	//     printf("Not OK\n");

	return 0;
}

int csloader_delete_file(struct sp_port *port, const char *filename)
{
	// printf("rmfile %s\n", filename);

	uint8_t data[1000] = {0};
	size_t size = 0;
	size += ascii_to_ucs2(filename, &data[size]);

	uint8_t cmd_buf[1000] = {0};
	int cmd_len = cmd_encode_csloader_packet(CMD_CS_FSX, CMD_FSX_RMFILE, data, size, cmd_buf);
	if (cmd_len <= 0)
		return -1;

	// --- send command
	if (serial_send_packetdata_ack(port, cmd_buf, cmd_len) < 0)
		return -1;

	uint8_t resp[8];
	int rcv_len = serial_read(port, resp, sizeof(resp), 10 * TIMEOUT);
	if (rcv_len <= 0)
		return -1;

	struct packetdata_t repl;
	if (cmd_decode_packet(resp, rcv_len, &repl) != 0)
		return -1;

	if (repl.cmd != CMD_CS_FSX) {
		fprintf(stderr, "Bad reply %02X expected %02X\n", repl.cmd, CMD_CS_FSX);
		return -1;
	}

	return 0;
}

int csloader_delete_directory(struct sp_port *port, const char *dirname)
{
	// printf("rmdir %s\n", dirname);

	uint8_t data[1000] = {0};
	size_t size = 0;
	size += ascii_to_ucs2(dirname, &data[size]);

	uint8_t cmd_buf[1000] = {0};
	int cmd_len = cmd_encode_csloader_packet(CMD_CS_FSX, CMD_FSX_RMDIR, data, size, cmd_buf);
	if (cmd_len <= 0)
		return -1;

	// --- send command
	if (serial_send_packetdata_ack(port, cmd_buf, cmd_len) < 0)
		return -1;

	uint8_t resp[8] = {0};
	int rcv_len = serial_read(port, resp, sizeof(resp), 10 * TIMEOUT);
	if (rcv_len <= 0)
		return -1;

	struct packetdata_t repl;
	if (cmd_decode_packet(resp, rcv_len, &repl) != 0)
		return -1;

	if (repl.cmd != CMD_CS_FSX) {
		fprintf(stderr, "Bad reply %02X expected %02X\n", repl.cmd, CMD_CS_FSX);
		return -1;
	}

	return 0;
}

int csloader_put_file(struct sp_port *port, const char *name, size_t filesize, const uint8_t *filebuffer)
{
	if (!name || !filebuffer) {
		fprintf(stderr, "Invalid argument(s) passed to csloader_put_file()\n");
		return -1;
	}

	size_t name_len = strlen(name);
	char namepart[1024] = {0};
	for (size_t i = 0; i < name_len; i++) {
		namepart[i] = (char)name[i];
		namepart[i + 1] = '\0';

		// mkdir if directory is not exist
		if (name[i + 1] == '/')
			csloader_make_directory(port, namepart);
		// remove old file if already exist
		else if (name[i + 1] == '\0')
			csloader_delete_file(port, namepart);
	}

	// printf("putfile %s\n", name);

	uint8_t cmd_buf[0x400] = {0};
	size_t offset = 0;
	printf("\rwrite file: %s %zu/%zu bytes ", name, offset, filesize);

	while (offset < filesize) {
		size_t chunk_size;
		size_t size = 0;

		// --- allocate enough room for metadata + chunk ---
		size_t max_alloc = 0x400; // safe upper bound per block
		uint8_t *data = malloc(max_alloc);
		if (!data) {
			fprintf(stderr, "Failed to allocate memory (size=%zu)\n", max_alloc);
			return -1;
		}

		// --- first block: include filename and metadata ---
		if (offset == 0) {
			size += ascii_to_ucs2(name, &data[size]);

			// attributes
			uint32_t attr = (OSEATTR_READABLE | OSEATTR_WRITEABLE | OSEATTR_EXECUTABLE);
			data[size++] = (uint8_t)(attr);
			data[size++] = (uint8_t)(attr >> 8);
			data[size++] = (uint8_t)(attr >> 16);
			data[size++] = (uint8_t)(attr >> 24);

			// total file size (little endian)
			data[size++] = (uint8_t)(filesize);
			data[size++] = (uint8_t)(filesize >> 8);
			data[size++] = (uint8_t)(filesize >> 16);
			data[size++] = (uint8_t)(filesize >> 24);

			// compute chunk size = 0x200 - header
			chunk_size = 0x200 - size;
			if (chunk_size > filesize)
				chunk_size = filesize;
		} else {
			// subsequent chunks are up to 0x400 bytes
			chunk_size = (filesize - offset > 0x400) ? 0x400 : (filesize - offset);
		}

		// --- copy file data ---
		memcpy(&data[size], &filebuffer[offset], chunk_size);
		size += chunk_size;

		// --- encode into CS packet ---
		int cmd_len = cmd_encode_csloader_packet(CMD_CS_FSX, CMD_FSX_PUTFILE, data, size, cmd_buf);
		free(data);

		if (cmd_len <= 0)
			return -1;

		// --- send command
		if (serial_send_packetdata_ack(port, cmd_buf, cmd_len) < 0)
			return -1;

		uint8_t resp[8] = {0};
		int rcv_len = serial_wait_packet(port, resp, sizeof(resp), 10 * TIMEOUT);
		if (rcv_len <= 0)
			return -1;

		struct packetdata_t repl;
		if (cmd_decode_packet(resp, rcv_len, &repl) != 0)
			return -1;

		if (repl.cmd != CMD_CS_FSX) {
			fprintf(stderr, "Bad reply %02X expected 0x03\n", repl.cmd);
			return -1;
		}

		offset += chunk_size;
		printf("\rwrite file: %s %zu/%zu bytes ", name, offset, filesize);
	}

	printf("\n");

	return 0;
}

int csloader_get_file(struct sp_port *port, const char *local_path, const char *fname)
{
	uint8_t cmd_buf[0x100] = {0};
	uint8_t data[0x256] = {0};
	size_t size = 0;

	// --- Encode filename (UCS2) ---
	size += ascii_to_ucs2(fname, &data[size]);

	// --- Build FSX GETFILE command ---
	int cmd_len = cmd_encode_csloader_packet(CMD_CS_FSX, CMD_FSX_GETFILE, data, size, cmd_buf);
	if (cmd_len <= 0)
		return -1;

	// --- Send command ---
	if (serial_send_packetdata_ack(port, cmd_buf, cmd_len) < 0)
		return -1;

	uint8_t resp[0x1000 + 7] = {0};
	int rcv_len = serial_wait_packet(port, resp, sizeof(resp), 10 * TIMEOUT);
	if (rcv_len <= 0)
		return -1;

	// --- Get file size ---
	uint32_t fsize = get_word(&resp[7]);
	uint32_t buf_size = 0x1000 - 0x7;

	// --- Small file fits entirely in first packet ---
	if (fsize < buf_size) {
		FILE *out = fopen(local_path, "wb");
		if (!out)
			return -1;

		printf("\rread file: %s %u bytes", fname, fsize);
		fflush(stdout);
		fwrite(&resp[0xB], 1, fsize, out);
		fclose(out);
		printf("\n");
		return 0;
	}

	// --- Multi-chunk path ---
	struct packetdata_t repl;
	if (cmd_decode_packet(resp, rcv_len, &repl) != 0)
		return -1;

	uint8_t *file_buf = malloc(fsize);
	if (!file_buf)
		return -1;

	uint32_t offset = 0;
	uint32_t first_chunk = repl.length - 6;

	memcpy(file_buf + offset, &repl.data[6], first_chunk);
	offset += first_chunk;

	while (offset < fsize) {
		if (serial_send_ack(port) != 0)
			goto exit_err;

		rcv_len = serial_read(port, resp, sizeof(resp), TIMEOUT);
		if (rcv_len <= 0)
			goto exit_err;

		if (cmd_decode_packet(resp, rcv_len, &repl) != 0)
			goto exit_err;

		if (repl.data[1] != 0) {
			printf("\nBad data: %02X\n", repl.data[1]);
			goto exit_err;
		}

		uint32_t chunk_size = repl.length - 2;
		if (offset + chunk_size > fsize)
			chunk_size = fsize - offset;

		memcpy(file_buf + offset, &repl.data[2], chunk_size);
		offset += chunk_size;

		printf("\rread file: %s %u/%u bytes ", fname, offset, fsize);
		fflush(stdout);
	}

	FILE *out = fopen(local_path, "wb");
	if (!out)
		goto exit_err;

	fwrite(file_buf, 1, fsize, out);
	fclose(out);

	printf("\n");
	free(file_buf);
	return 0;

exit_err:
	free(file_buf);
	return -1;
}

int csloader_read_directory(struct sp_port *port, const char *src_dir, const char *dest_dir)
{
	// --- Create local directory ---
	MKDIR(dest_dir);

	// --- Step 1: change & confirm directory ---
	if (csloader_change_directory(port, src_dir) != 0)
		return -1;

	char cwd[256];
	int cwd_len = csloader_get_working_directory(port, cwd, sizeof(cwd));
	if (cwd_len <= 0)
		return -1;

	printf("read directory: %s\n", cwd);

	cs_entry_t *entries = NULL;
	size_t count = 0;
	if (csloader_list(port, &entries, &count) == 0) {
		for (size_t i = 0; i < count; i++) {
			const cs_entry_t *e = &entries[i];

			if (e->d_type) {
				// --- Build full paths for recursion ---
				char new_src[512], new_dest[512];
				if (strcmp(src_dir, "/") == 0)
					snprintf(new_src, sizeof(new_src), "/%s", e->name);
				else
					snprintf(new_src, sizeof(new_src), "%s/%s", src_dir, e->name);
				snprintf(new_dest, sizeof(new_dest), "%s/%s", dest_dir, e->name);

				// --- Recursively download subdirectory ---
				csloader_read_directory(port, new_src, new_dest);

				// --- Go back to previous directory after recursion
				csloader_change_directory(port, cwd);
			} else {
				// --- Build full local path ---
				char local_path[512];
				snprintf(local_path, sizeof(local_path), "%s/%s", dest_dir, e->name);
				csloader_get_file(port, local_path, e->name);
			}
		}
	}

	free(entries);
	return 0;
}

int csloader_write_directory(struct sp_port *port, const char *src_dir, const char *src_subdir, const char *dest_dir)
{
	size_t filesize = 0;
	uint8_t *filebuff = NULL;
	char local_filename[4096];
	char phone_filename[4096];
	char basedirname[2048];
	struct dirent **namelist = NULL;

	// Build current local directory
	if (strlen(src_subdir) > 0)
		snprintf(basedirname, sizeof(basedirname), "%s/%s", src_dir, src_subdir);
	else
		snprintf(basedirname, sizeof(basedirname), "%s", src_dir);

	int n = scandir(basedirname, &namelist, 0, alphasort);
	if (n < 0) {
		fprintf(stderr, "Error scanning directory %s\n", basedirname);
		return -1;
	}

	for (int i = 0; i < n; i++) {
		const char *entry = namelist[i]->d_name;
		if (entry[0] == '.') {
			free(namelist[i]);
			continue;
		}

		char local_path[4096];
		snprintf(local_path, sizeof(local_path), "%s/%s", basedirname, entry);

		if (is_directory(local_path)) {
			char newsub[4096];
			if (strlen(src_subdir) > 0)
				snprintf(newsub, sizeof(newsub), "%s/%s", src_subdir, entry);
			else
				snprintf(newsub, sizeof(newsub), "%s", entry);

			if (csloader_write_directory(port, src_dir, newsub, dest_dir) < 0) {
				free(namelist[i]);
				free(namelist);
				return -1;
			}
		} else {
			// --- Build full local filename ---
			if (strlen(src_subdir) > 0)
				snprintf(local_filename, sizeof(local_filename), "%s/%s/%s", src_dir, src_subdir,
				         entry);
			else
				snprintf(local_filename, sizeof(local_filename), "%s/%s", src_dir, entry);

			// --- Derive phone path ---
			const char *rel_path = local_filename + strlen(src_dir);
			while (*rel_path == '/' || *rel_path == '\\')
				rel_path++; // strip leading slash

			if (strcmp(dest_dir, "/") == 0)
				snprintf(phone_filename, sizeof(phone_filename), "/%s", rel_path);
			else
				snprintf(phone_filename, sizeof(phone_filename), "%s/%s", dest_dir, rel_path);

			filebuff = load_file(local_filename, &filesize);
			if (!filebuff) {
				fprintf(stderr, "Failed to read %s\n", local_filename);
				free(namelist[i]);
				continue;
			}

			int ret = 0;
			if (filesize == 0)
				ret = csloader_delete_file(port, phone_filename);
			else
				ret = csloader_put_file(port, phone_filename, filesize, filebuff);

			free(filebuff);
			if (ret < 0) {
				free(namelist[i]);
				free(namelist);
				return -1;
			}
		}

		free(namelist[i]);
	}

	free(namelist);
	return 0;
}

int csloader_upload_zip(struct sp_port *port, const char *zip_filename)
{
	const char *tmp_dir = "./tmp";

	// --- clean tmp dir ---
	remove_recursive(tmp_dir);
	MKDIR(tmp_dir);

	printf("Extracting %s to %s ...\n", zip_filename, tmp_dir);
	if (zip_extract(zip_filename, tmp_dir) < 0) {
		fprintf(stderr, "Failed to extract ZIP archive\n");
		return -1;
	}

	printf("Uploading extracted files...\n");
	int ret = csloader_write_directory(port, tmp_dir, "", "/");

	// optional: cleanup after upload
	remove_recursive(tmp_dir);

	return ret;
}

int csloader_shutdown_fsx_server(struct sp_port *port)
{
	// shutdown
	printf("Shutdown FSX server... ");

	uint8_t cmd_buf[8];
	int cmd_len = cmd_encode_csloader_packet(CMD_CSLOADER, CMD_CS_SHUTDOWN, NULL, 0, cmd_buf);
	if (cmd_len <= 0)
		return -1;

	if (serial_send_packetdata_ack(port, cmd_buf, cmd_len) < 0)
		return -1;

	uint8_t resp[8];
	int rcv_len = serial_wait_packet(port, resp, sizeof(resp), 10 * TIMEOUT);
	if (rcv_len <= 0) {
		printf("failed\n\n");
		return -1;
	}

	printf("OK\n\n");

	return 0;
}
