#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <libserialport.h>

#include "common.h"
#include "cmd.h"
#include "loader.h"
#include "gdfs.h"
#include "serial.h"

int csloader_read_gdfs_var(struct sp_port *port, uint8_t block, uint8_t lo, uint8_t hi,
                           uint8_t *dest, int maxdest, int binary)
{
    uint8_t gdfs_var[3];
    gdfs_var[0] = block;
    gdfs_var[1] = lo;
    gdfs_var[2] = hi;

    uint8_t cmd_buf[32];
    int cmd_len = cmd_encode_csloader_packet(0x04, 0x01, gdfs_var, 3, cmd_buf);
    if (cmd_len <= 0)
        return -1;

    if (serial_send_packetdata_ack(port, cmd_buf, cmd_len) < 0)
        return -1;

    uint8_t resp[0x1000];
    int rcv_len = serial_wait_packet(port, resp, sizeof(resp), TIMEOUT);
    if (rcv_len <= 0)
        return -1;

    if (resp[0] != SERIAL_ACK || resp[1] != SERIAL_HDR89 || resp[2] != 0x04)
    {
        fprintf(stderr, "Bad reply: %X %X %X expected:06 89 04\n", resp[0], resp[1], resp[2]);
        return 0;
    }

    int len = get_half(&resp[3]);

    int gdfs_len = len - 2;
    if (gdfs_len > maxdest)
        gdfs_len = maxdest;

    if (binary)
        memcpy(dest, &resp[7], gdfs_len);
    else
        strncpy((char *)dest, (char *)&resp[6], gdfs_len);

    return gdfs_len;
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

    int cmd_len = cmd_encode_csloader_packet(0x04, 0x03, gdfs_var, size + 7, cmd_buf);
    free(gdfs_var);

    if (cmd_len <= 0)
        return -1;

    if (serial_send_packetdata_ack(port, cmd_buf, cmd_len) < 0)
        return -1;

    if (serial_wait_ack(port, 50 * TIMEOUT) != 0)
        return -1;

    int rcv_len = serial_wait_packet(port, resp, sizeof(resp), TIMEOUT);
    if (rcv_len <= 0)
        return -1;

    struct packetdata_t repl;
    if (cmd_decode_packet(resp, rcv_len, &repl) != 0)
        return -1;

    if (repl.data[0] != 0xFF && repl.data[1] != 0x00)
        return -1;

    return 0;
}

int csloader_write_gdfs(struct sp_port *port, const char *inputfname)
{
    printf("Restore GDFS...\n");
    FILE *f = fopen(inputfname, "rb");
    if (!f)
    {
        printf("Restore: Error (Couldn't open inputfile!)\n");
        return -1;
    }

    fseek(f, 0, SEEK_END);
    uint32_t datasize = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t *buf = malloc(datasize);
    fread(buf, 1, datasize, f);
    fclose(f);
    uint32_t readbytes = 0;
    uint8_t *bufpointer = &buf[4];

    datasize -= 4;
    uint32_t varcount = get_word(&buf[0]);
    printf("Attempting to write %i variables...\n", varcount);
    varcount = 0;
    while (readbytes < datasize)
    {
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

        if (csloader_write_gdfs_var(port, block, lo, hi, bufpointer, realsize) != 0)
        {
            free(buf);
            printf("Wrote %i units!\n", varcount);
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

    int cmd_len = cmd_encode_csloader_packet(0x04, 0x02, NULL, 0, cmd_buf);
    if (cmd_len <= 0)
        return -1;

    if (serial_send_packetdata_ack(port, cmd_buf, cmd_len) < 0)
        return -1;

    if (serial_wait_ack(port, 100 * TIMEOUT) < 0)
        return -1;

    int rcv_len = serial_wait_packet(port, &resp[0], 10, 500 * TIMEOUT);
    if (rcv_len <= 0)
        return -1;

    uint32_t datasize = (resp[2] | (resp[3] << 8)) + 1;
    uint32_t varcount = get_word(&resp[6]);

    printf("stated number of vars: %d\n", varcount);

    char outfile[512];
    snprintf(outfile, sizeof(outfile), "./backup/GDFS_%s_%s.bin", phone->phone_name, phone->otp_imei);

    FILE *out = fopen(outfile, "wb");
    if (!out)
    {
        fprintf(stderr, "Can not open backup file\n");
        return -1;
    }

    uint32_t bytesread = 10;
    uint32_t chunkcount = 0;
    for (uint32_t i = 0; i < varcount; i++)
    {
        if (i == 0)
            serial_wait_packet(port, &resp[bytesread], 1, 500 * TIMEOUT); // wait longer for first unit
        else
            serial_wait_packet(port, &resp[bytesread], 1, 100 * TIMEOUT);

        if (bytesread >= datasize)
        {
            printf("Found: %d units\n", chunkcount);
            fwrite(&resp[6], sizeof(uint8_t), bytesread - 6, out);
            serial_send_ack(port);
            serial_wait_packet(port, resp, 6, 50 * TIMEOUT);
            datasize = (resp[2] | (resp[3] << 8)) + 1;
            bytesread = 6;
            chunkcount = 0;
            serial_wait_packet(port, &resp[bytesread], 1, 100 * TIMEOUT);
        }
        bytesread++;
        serial_wait_packet(port, &resp[bytesread], 6, 100 * TIMEOUT);
        bytesread += 2;
        uint32_t varsize = get_word(&resp[bytesread]);
        bytesread += 4;
        serial_wait_packet(port, &resp[bytesread], varsize, 100 * TIMEOUT);

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
    serial_wait_packet(port, &byteleft, 1, 50 * TIMEOUT);

    return 0;
}

int csloader_parse_gdfs_script(struct sp_port *port, const char *inputfname, const char *outputfname)
{
    printf("\nRun GDFS-script...%s\n", inputfname);

    FILE *f = fopen(inputfname, "r");
    if (!f)
    {
        fprintf(stderr, "GDFS-Script: Error (Couldn't open inputfile!)\n");
        return -1;
    }

    FILE *fout = fopen(outputfname, "w");
    if (!fout)
    {
        fprintf(stderr, "GDFS-Script: Error (Couldn't open outputfile!)\n");
        fclose(f);
        return -1;
    }

    // Always write header at top
    time_t now = time(NULL);
    char timestr[64];
    strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", localtime(&now));
    fprintf(fout, "; Created with seftool\n; Creation time and date: %s\n\n", timestr);

    char linebuf[4096];
    uint32_t varcount = 0, varreadcount = 0;

    while (fgets(linebuf, sizeof(linebuf), f))
    {
        // skip empty lines and comments
        if (linebuf[0] == '\0' ||
            linebuf[0] == '\n' ||
            linebuf[0] == '#' ||
            linebuf[0] == ';')
            continue;

        if (strncmp(linebuf, "gdfswrite:", 10) == 0)
        {
            uint32_t block, hi, lo;
            char datahex[0x680 * 2 + 1] = {0}; // hex string buffer

            // Parse block, hi, lo, and optional datahex
            int n = sscanf(linebuf + 10, "%4x%2x%2x%[0-9A-Fa-f]", &block, &hi, &lo, datahex);

            if (n >= 3) // at least block+hi+lo parsed
            {
                // Convert hex string to bytes
                uint8_t vardata[0x680];
                size_t datalen = 0;

                for (char *p = datahex; *p && *(p + 1); p += 2)
                {
                    uint32_t byte;
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
        }
        else if (strncmp(linebuf, "gdfsread:", 9) == 0)
        {
            uint32_t block, hi, lo;
            if (sscanf(linebuf + 9, "%4x%2x%2x", &block, &hi, &lo) == 3)
            {
                uint8_t vardata[0x10000] = {0};
                int len = csloader_read_gdfs_var(port, block, lo, hi, vardata, sizeof(vardata), 1);

                printf("Reading GDFS var %.04X/%02X%02X\n", block, hi, lo);
                fprintf(fout, "gdfswrite:%04X%02X%02X", block, hi, lo);
                for (int i = 0; i < len; i++)
                    fprintf(fout, "%02X", vardata[i]);
                fprintf(fout, "\n");

                varreadcount++;
            }
        }
        else
        {
            fprintf(stderr, "Warning: Unknown or unsupported script line skipped: %s", linebuf);
            continue;
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
