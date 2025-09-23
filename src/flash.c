#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <libserialport.h>

#ifdef _WIN32
#include <io.h>
#define access _access
#else
#include <unistd.h>
#endif

#include "babe.h"
#include "common.h"
#include "cmd.h"
#include "flash.h"
#include "loader.h"
#include "gdfs.h"
#include "serial.h"

int flash_read(struct sp_port *port, struct phone_info *phone,
               uint32_t memaddr, int size)
{
    // --- Build request payload [start, end]
    uint32_t addr_range[2];
    addr_range[0] = memaddr;
    addr_range[1] = memaddr + size;

    uint8_t cmd_buf[64];
    int cmd_len = cmd_encode_binary_packet(0x32,
                                           (uint8_t *)addr_range,
                                           sizeof(addr_range),
                                           cmd_buf);
    if (cmd_len <= 0)
        return -1;

    // --- Send request
    if (serial_send_packetdata_ack(port, cmd_buf, cmd_len) < 0)
        return -1;

    uint8_t ack;
    int rcv_len = serial_wait_packet(port, &ack, 1, 50 * TIMEOUT);
    if (rcv_len <= 0)
        return -1;

    char outfile[1024];
    snprintf(outfile, sizeof(outfile),
             "./backup/flashdump_%s_%08X_%08X.bin",
             phone->otp_imei,
             addr_range[0], size);

    FILE *out = fopen(outfile, "wb");
    if (!out)
    {
        perror("open output");
        return -1;
    }

    printf("\nreading raw: %s\n", outfile);

    // --- block progress
    int total_blocks = (size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    printf("reading %d blocks\n", total_blocks);

    // --- Receive response loop
    int pos = 0;
    while (pos < size)
    {
        // read 4-byte header
        uint8_t hdr_buf[4];
        int rcv_len = serial_wait_packet(port, hdr_buf, sizeof(hdr_buf), 5 * TIMEOUT);
        if (rcv_len < 0)
        {
            fclose(out);
            return -1;
        }

        int hdr = hdr_buf[0];               // must be 0x89
        int cmd = hdr_buf[1];               // command ID
        int length = get_half(&hdr_buf[2]); // little endian u16

        if (hdr != SERIAL_HDR89)
        {
            fprintf(stderr, "Bad HDR: 0x%02X\n", hdr);
            fclose(out);
            return -1;
        }

        // Error packet (type = 0x09)
        if (cmd == 0x09)
        {
            fprintf(stderr, "Bad CMD: %X\n", cmd);
            fclose(out);
            return -1;
        }

        // Must be data reply (type = 0x33)
        if (cmd != 0x33)
        {
            fprintf(stderr, "Unexpected CMD 0x33 got 0x%X\n", cmd);
            fclose(out);
            return -1;
        }

        if (length < 6)
        {
            fprintf(stderr, "Bad reply size\n");
            fclose(out);
            return -1;
        }

        // read payload
        uint8_t resp[0x800];
        rcv_len = serial_wait_packet(port, resp, length, 5 * TIMEOUT);
        if (rcv_len < 0)
        {
            fclose(out);
            return -1;
        }

        // read checksum
        uint8_t checksum;
        rcv_len = serial_wait_packet(port, &checksum, 1, 5 * TIMEOUT);
        if (rcv_len < 0)
        {
            fclose(out);
            return -1;
        }

        // calculate XOR of header + payload
        uint8_t sum = 0;
        for (int i = 0; i < 4; i++)
            sum ^= hdr_buf[i];
        for (int i = 0; i < length; i++)
            sum ^= resp[i];
        sum = (sum + 7) & 0xFF;

        if (sum == checksum)
        {
            uint8_t ack = SERIAL_ACK;
            if (serial_write(port, &ack, 1) < 0)
            {
                fclose(out);
                return -1;
            }
        }
        else
        {
            uint8_t nak = SERIAL_NAK;
            serial_write(port, &nak, 1);
            fprintf(stderr, "Checksum mismatch! 0x%x x 0x%x\n", checksum, sum);
            fclose(out);
            return -1;
        }

        // parse address
        uint32_t rpl_addr = get_word(&resp[2]);
        if (rpl_addr != memaddr + pos)
        {
            fprintf(stderr, "Bad reply addr: expected 0x%08X got 0x%08X\n",
                    memaddr + pos, rpl_addr);
            fclose(out);
            return -1;
        }

        // actual data length = length - 6 (address header inside payload)
        int data_len = length - 6;
        if (pos + data_len > size)
            data_len = size - pos; // truncate on last block

        fwrite(&resp[6], 1, data_len, out);
        pos += data_len;

        // ---- live progress print (immediate) ----
        int current_block = pos / BLOCK_SIZE;
        if (current_block < total_blocks)
        {
            uint32_t block_addr = memaddr + current_block * BLOCK_SIZE;
            printf("\rreading block: %d/%d (addr 0x%08X size 0x10000)",
                   current_block + 1, total_blocks, block_addr);
            fflush(stdout);
        }
    }

    printf("\n\n");

    fclose(out);
    return 0;
}

#define FLASH_OK 0
#define FLASH_ERROR -1

int flash_babe(struct sp_port *port, uint8_t *addr, long size, int flashfull)
{
    struct babehdr_t *hdr = (struct babehdr_t *)addr;
    int fileformatver = hdr->ver;
    int hashsize = fileformatver >= 4 ? 20 : 1;
    int blocks = hdr->payloadsize1;
    int hdrsize = (fileformatver <= 2) ? 0x480 : blocks * hashsize + 0x380;

    // --- send header ---
    int curpos = 0;
    while (curpos < hdrsize)
    {
        int chunk = hdrsize - curpos;
        if (chunk > 0x800)
            chunk = 0x800;

        uint8_t cmd_buf[0x1000];
        int cmd_len = cmd_encode_binary_packet(0x0E, addr + curpos, chunk, cmd_buf);
        if (cmd_len <= 0)
            return FLASH_ERROR;

        if (serial_send_packetdata_ack(port, cmd_buf, cmd_len) < 0)
            return FLASH_ERROR;

        uint8_t resp[7];
        int rcv_len = serial_wait_packet(port, resp, sizeof(resp), 5 * TIMEOUT);
        if (rcv_len <= 0)
            return FLASH_ERROR;

        struct packetdata_t repl;
        if (cmd_decode_packet(resp, rcv_len, &repl) != 0)
            return FLASH_ERROR;

        if (repl.cmd != 0x0F || repl.length != 1 || repl.data[0] != 0x00)
        {
            printf("send header error\n");
            return FLASH_ERROR;
        }
        curpos += chunk;
    }

    // --- detect real block count ---
    curpos = hdrsize;
    int bl;
    for (bl = 0; bl < blocks; bl++)
    {
        if (curpos + 8 >= size)
            break;
        long bsize = ((long *)(addr + curpos))[1];
        if (bsize > BLOCK_SIZE)
            break;
        curpos += 8;
        if (curpos + bsize > size)
            break;
        curpos += bsize;
    }
    blocks = bl;
    printf("flashing %d blocks\n", blocks);

    // --- flash blocks ---
    curpos = hdrsize;
    for (bl = 0; bl < blocks; bl++)
    {
        if (curpos + 8 >= size)
            break;

        int addr_val = ((int *)(addr + curpos))[0];
        int bsize = ((int *)(addr + curpos))[1];

        printf("\rflashing block %d/%d (addr %08X size %08X)",
               bl + 1, blocks, addr_val, bsize);

        // send block header
        uint8_t cmd_buf[0x1000];
        int cmd_len = cmd_encode_binary_packet(0x10, addr + curpos, 8, cmd_buf);
        if (cmd_len <= 0)
            return FLASH_ERROR;

        if (serial_send_packetdata_ack(port, cmd_buf, cmd_len) < 0)
            return FLASH_ERROR;

        if (serial_wait_ack(port, TIMEOUT) < 0)
            return FLASH_ERROR;

        if (bsize > BLOCK_SIZE)
            break;

        curpos += 8;

        if ((curpos + bsize > size))
            break;

        // send block data in chunks
        while (bsize > 0)
        {
            uint32_t tsize = (bsize > 0x800) ? 0x800 : bsize;
            cmd_len = cmd_encode_binary_packet(0x01, addr + curpos, tsize, cmd_buf);
            if (cmd_len <= 0)
                return FLASH_ERROR;
            if (serial_write_chunks(port, cmd_buf, cmd_len, 0x180) < 0)
                return FLASH_ERROR;
            if (serial_wait_ack(port, TIMEOUT) < 0)
                return FLASH_ERROR;

            curpos += tsize;
            bsize -= tsize;
        }

        // wait for block reply
        uint8_t resp[8];
        int rcv_len = serial_wait_packet(port, resp, sizeof(resp), TIMEOUT);
        if (rcv_len <= 0)
            return FLASH_ERROR;

        struct packetdata_t repl;
        if (cmd_decode_packet(resp, rcv_len, &repl) != 0)
            return FLASH_ERROR;

        switch (repl.cmd)
        {
        case 0x13:
            if (repl.length != 1 || repl.data[0] != 0x00)
            {
                printf("\nsend block error\n");
                return FLASH_ERROR;
            }
            break;
        default:
            printf("\nunexpected reply during flash block: 0x%02X\n", repl.cmd);
            return FLASH_ERROR;
        }
    }

    // --- finalization ---
    if (flashfull)
    {
        uint8_t cmd_buf[0x100];
        int cmd_len = cmd_encode_binary_packet(0x11, NULL, 0, cmd_buf);
        if (cmd_len <= 0)
            return FLASH_ERROR;

        if (serial_send_packetdata_ack(port, cmd_buf, cmd_len) < 0)
            return FLASH_ERROR;

        uint8_t resp[8];
        int rcv_len = serial_wait_packet(port, resp, sizeof(resp), 200 * TIMEOUT); // 20s timeout
        if (rcv_len <= 0)
            return FLASH_ERROR;

        struct packetdata_t repl;
        if (cmd_decode_packet(resp, rcv_len, &repl) != 0)
            return FLASH_ERROR;

        switch (repl.cmd)
        {
        case 0x12:
            if (repl.length != 1 || repl.data[0] != 0x00)
            {
                printf("final error\n");
                return FLASH_ERROR;
            }
            break;
        default:
            printf("unexpected reply during final check: 0x%02X\n", repl.cmd);
            return FLASH_ERROR;
        }
    }

    printf("\n\n%d blocks flashed ok\n", blocks);

    return FLASH_OK;
}

int flash_babe_fw(struct sp_port *port, const char *filename, int flashfull)
{
    printf("\nflashing babe: %s\n", filename);

    FILE *fb = fopen(filename, "rb");
    if (!fb)
    {
        fprintf(stderr, "file (%s) does not exist!\n", filename);
        return -1;
    }

    // Read entire loader into memory
    fseek(fb, 0, SEEK_END);
    long size = ftell(fb);
    fseek(fb, 0, SEEK_SET);

    uint8_t *addr = malloc(size);
    if (!addr)
    {
        fclose(fb);
        fprintf(stderr, "No memory (%li bytes)\n", size);
        return -1;
    }

    fread(addr, 1, size, fb);
    fclose(fb);

    int babe = babe_check(addr, size, CHECKBABE_CHECKFULL);
    switch (babe)
    {
    case CHECKBABE_NOTBABE:
        printf("This is not BABE file\n");
        goto exit_error;

    case CHECKBABE_BADFILE:
        printf("Bad BABE file\n");
        goto exit_error;

    case CHECKBABE_CANTCHECK:
        printf("Can not check BABE file\n");
        goto exit_error;

    case CHECKBABE_NOTFULL:
        printf("This is not full BABE file\n");
        goto exit_error;

    case CHECKBABE_OK:
        // valid, continue flashing
        break;

    default:
        printf("Unknown BABE check result: %d\n", babe);
        goto exit_error;
    }

    if (flash_babe(port, addr, size, flashfull) == 0)
    {
        free(addr);
        return FLASH_OK;
    }

exit_error:
    free(addr);
    return FLASH_ERROR;
}

// TODO
int flash_raw(struct sp_port *port, const char *filename, uint32_t rawaddr)
{
    printf("\nflashing raw: %s\n", filename);

    FILE *fb = fopen(filename, "rb");
    if (!fb)
    {
        fprintf(stderr, "file (%s) does not exist!\n", filename);
        return -1;
    }

    // Read entire loader into memory
    fseek(fb, 0, SEEK_END);
    long size = ftell(fb);
    fseek(fb, 0, SEEK_SET);

    uint8_t *raw = malloc(size);
    if (!raw)
    {
        fclose(fb);
        fprintf(stderr, "No memory (%li bytes)\n", size);
        return -1;
    }

    fread(raw, 1, size, fb);
    fclose(fb);

    printf("converting raw->babe\n");
    long numblocks = (size + 0xFFFF) / BLOCK_SIZE;
    long babesize = numblocks * (8 + 1) + size + sizeof(struct babehdr_t);

    uint8_t *babe = malloc(babesize);
    struct babehdr_t *babe_hdr = (struct babehdr_t *)babe;

    babe_hdr->sig = 0xBEBA;
    babe_hdr->ver = 3;
    babe_hdr->payloadsize1 = numblocks;
    long pos = sizeof(struct babehdr_t) + numblocks;
    uint8_t *rawp = raw;
    while (size)
    {
        int bsize = size > BLOCK_SIZE ? BLOCK_SIZE : size;
        *((uint32_t *)(babe + pos)) = rawaddr;
        pos += 4;
        *((uint32_t *)(babe + pos)) = bsize;
        pos += 4;
        memcpy(babe + pos, rawp, bsize);
        pos += bsize;
        rawp += bsize;
        size -= bsize;
        rawaddr += bsize;
    }

    int ret = flash_babe(port, babe, babesize, 1);
    free(raw);
    free(babe);
    return ret;
}