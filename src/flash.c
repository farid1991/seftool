#include <ctype.h>
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

#define FLASH_OK 0
#define FLASH_ERROR -1

#define FLASH_TIMEOUT 50

// ------------- Write to Flash -------------

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
        int rcv_len = serial_wait_packet(port, resp, sizeof(resp), FLASH_TIMEOUT);
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
        fflush(stdout);

        // send block header
        uint8_t cmd_buf[0x1000];
        int cmd_len = cmd_encode_binary_packet(0x10, addr + curpos, 8, cmd_buf);
        if (cmd_len <= 0)
            return FLASH_ERROR;

        if (serial_send_packetdata_ack(port, cmd_buf, cmd_len) < 0)
            return FLASH_ERROR;

        if (serial_wait_ack(port, FLASH_TIMEOUT) < 0)
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
            if (serial_write_chunks(port, cmd_buf, cmd_len, 0x400) < 0)
                return FLASH_ERROR;
            if (serial_wait_ack(port, FLASH_TIMEOUT) < 0)
                return FLASH_ERROR;

            curpos += tsize;
            bsize -= tsize;
        }

        // wait for block reply
        uint8_t resp[8];
        int rcv_len = serial_wait_packet(port, resp, sizeof(resp), FLASH_TIMEOUT);
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
        int rcv_len = serial_wait_packet(port, resp, sizeof(resp), 100 * TIMEOUT); // 10s timeout
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
        return FLASH_ERROR;
    }

    // Read entire loader into memory
    fseek(fb, 0, SEEK_END);
    size_t size = ftell(fb);
    fseek(fb, 0, SEEK_SET);

    uint8_t *addr = malloc(size);
    if (!addr)
    {
        fclose(fb);
        fprintf(stderr, "No memory (%zu bytes)\n", size);
        return FLASH_ERROR;
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

uint8_t *raw_to_babe(uint8_t *raw, size_t size, uint32_t rawaddr, size_t *babe_size_out)
{
    size_t numblocks = (size + 0xFFFF) / BLOCK_SIZE;
    size_t babesize = numblocks * (8 + 1) + size + sizeof(struct babehdr_t);

    uint8_t *babe = malloc(babesize);
    if (!babe)
        return NULL;

    struct babehdr_t *babe_hdr = (struct babehdr_t *)babe;
    babe_hdr->sig = 0xBEBA;
    babe_hdr->ver = 3;
    babe_hdr->payloadsize1 = numblocks;

    size_t pos = sizeof(struct babehdr_t) + numblocks;
    uint8_t *rawp = raw;
    size_t left = size;

    while (left > 0)
    {
        int bsize = left > BLOCK_SIZE ? BLOCK_SIZE : left;
        *((uint32_t *)(babe + pos)) = rawaddr;
        pos += 4;
        *((uint32_t *)(babe + pos)) = bsize;
        pos += 4;
        memcpy(babe + pos, rawp, bsize);
        pos += bsize;

        rawp += bsize;
        left -= bsize;
        rawaddr += bsize;
    }

    if (babe_size_out)
        *babe_size_out = babesize;

    return babe;
}

int raw_to_babe_file(const char *raw_filename, const char *babe_filename, uint32_t rawaddr)
{
    FILE *fr = fopen(raw_filename, "rb");
    if (!fr)
    {
        fprintf(stderr, "file (%s) does not exist!\n", raw_filename);
        return FLASH_ERROR;
    }

    // Get file size
    fseek(fr, 0, SEEK_END);
    size_t size = ftell(fr);
    fseek(fr, 0, SEEK_SET);

    uint8_t *raw = malloc(size);
    if (!raw)
    {
        fclose(fr);
        fprintf(stderr, "No memory (%zu bytes)\n", size);
        return FLASH_ERROR;
    }

    if (fread(raw, 1, size, fr) != size)
    {
        fclose(fr);
        free(raw);
        fprintf(stderr, "Failed to read entire file (%s)\n", raw_filename);
        return FLASH_ERROR;
    }
    fclose(fr);

    // Convert
    size_t babe_size;
    uint8_t *babe = raw_to_babe(raw, size, rawaddr, &babe_size);
    free(raw);

    if (!babe)
    {
        fprintf(stderr, "Failed to allocate babe buffer\n");
        return FLASH_ERROR;
    }

    // Write out babe
    FILE *fw = fopen(babe_filename, "wb");
    if (!fw)
    {
        free(babe);
        fprintf(stderr, "Failed to create babe file (%s)\n", babe_filename);
        return FLASH_ERROR;
    }

    if (fwrite(babe, 1, babe_size, fw) != babe_size)
    {
        fclose(fw);
        free(babe);
        fprintf(stderr, "Failed to write babe file (%s)\n", babe_filename);
        return FLASH_ERROR;
    }

    fclose(fw);
    free(babe);

    printf("Converted %s -> %s (%zu bytes babe)\n", raw_filename, babe_filename, babe_size);
    return FLASH_OK;
}

int flash_raw(struct sp_port *port, const char *filename, uint32_t rawaddr)
{
    printf("\nflashing raw: %s\n", filename);

    FILE *fb = fopen(filename, "rb");
    if (!fb)
    {
        fprintf(stderr, "file (%s) does not exist!\n", filename);
        return FLASH_ERROR;
    }

    fseek(fb, 0, SEEK_END);
    size_t size = ftell(fb);
    fseek(fb, 0, SEEK_SET);

    uint8_t *raw = malloc(size);
    if (!raw)
    {
        fclose(fb);
        fprintf(stderr, "No memory (%zu bytes)\n", size);
        return FLASH_ERROR;
    }

    fread(raw, 1, size, fb);
    fclose(fb);

    printf("converting raw->babe\n");
    size_t babesize;
    uint8_t *babe = raw_to_babe(raw, size, rawaddr, &babesize);
    free(raw);

    if (!babe)
    {
        fprintf(stderr, "Failed to allocate babe buffer\n");
        return FLASH_ERROR;
    }

    int ret = flash_babe(port, babe, babesize, 1);
    free(babe);
    return ret;
}

int flash_restore_boot_area(struct sp_port *port, struct phone_info *phone)
{
    if (flash_detect_fw_version(port, phone) != 0)
        return -1;

    printf("Restoring boot area\n");

    // Build expected REST filename
    char restfile[256];
    snprintf(restfile, sizeof(restfile), "./rest/%s.rest", phone->fw_version);

    // Check if file exists
    FILE *frest = fopen(restfile, "rb");
    if (frest)
    {
        fclose(frest);
        if (flash_babe_fw(port, restfile, 1) != 0)
            return -1;
    }
    else
    {
        fprintf(stderr, "Missing REST file: %s\n", restfile);
        // Try RAW
        char raw_file[256];
        snprintf(raw_file, sizeof(raw_file), "./rest/%s.raw", phone->fw_version);
        FILE *fraw = fopen(raw_file, "rb");
        if (!fraw)
        {
            fprintf(stderr, "Missing RAW file: %s\n", raw_file);
            return -1;
        }
        fclose(fraw);

        if (phone->chip_id == PNX5230)
        {
            if (flash_raw(port, raw_file, 0x20100000) != 0)
                return -1;
        }
        else
        {
            if (flash_raw(port, raw_file, 0x44140000) != 0)
                return -1;
        }
    }

    return 0;
}

// ------------- Read from Flash -------------

// --- receive one flash block ---
int flash_recv_block(struct sp_port *port,
                     uint32_t expected_addr,
                     uint8_t *buf,
                     int maxlen)
{
    // read 4-byte header
    uint8_t hdr_buf[4];
    int rcv_len = serial_wait_packet(port, hdr_buf, sizeof(hdr_buf), 5 * TIMEOUT);
    if (rcv_len < 0)
        return FLASH_ERROR;

    int length = get_half(&hdr_buf[2]); // little endian u16

    if (hdr_buf[0] != SERIAL_HDR89)
    {
        fprintf(stderr, "Bad HDR: 0x%02X\n", hdr_buf[0]);
        return FLASH_ERROR;
    }
    if (hdr_buf[1] != 0x33)
    {
        fprintf(stderr, "Unexpected CMD 0x33 got 0x%X\n", hdr_buf[1]);
        return FLASH_ERROR;
    }
    if (length < 6)
    {
        fprintf(stderr, "Bad reply size\n");
        return FLASH_ERROR;
    }

    // read block
    uint8_t resp[0x800];
    rcv_len = serial_wait_packet(port, resp, length, 5 * TIMEOUT);
    if (rcv_len < 0)
        return FLASH_ERROR;

    // read checksum
    uint8_t checksum;
    rcv_len = serial_wait_packet(port, &checksum, 1, 5 * TIMEOUT);
    if (rcv_len < 0)
        return FLASH_ERROR;

    // verify checksum
    uint8_t sum = 0;
    for (int i = 0; i < 4; i++)
        sum ^= hdr_buf[i];
    for (int i = 0; i < length; i++)
        sum ^= resp[i];
    sum = (sum + 7) & 0xFF;

    if (sum != checksum)
    {
        uint8_t nak = SERIAL_NAK;
        serial_write(port, &nak, 1);
        fprintf(stderr, "Checksum mismatch! got 0x%02X expected 0x%02X\n", checksum, sum);
        return FLASH_ERROR;
    }

    // parse address
    uint32_t rpl_addr = get_word(&resp[2]);
    if (rpl_addr != expected_addr)
    {
        fprintf(stderr, "Bad reply addr: expected 0x%08X got 0x%08X\n",
                expected_addr, rpl_addr);
        return FLASH_ERROR;
    }

    // copy data into buf
    int data_len = length - 6; // subtract 4-byte addr + 2-byte length
    if (data_len > maxlen)
        data_len = maxlen; // truncate if needed

    memcpy(buf, &resp[6], data_len);
    return data_len;
}

static int flash_scan_fw_version(struct sp_port *port, struct phone_info *phone,
                                 uint32_t addr, size_t size)
{
    uint32_t addr_range[2] = {addr, addr + size};

    uint8_t cmd_buf[32];
    int cmd_len = cmd_encode_binary_packet(0x32,
                                           (uint8_t *)addr_range,
                                           sizeof(addr_range),
                                           cmd_buf);
    if (cmd_len <= 0)
        return FLASH_ERROR;

    if (serial_send_packetdata_ack(port, cmd_buf, cmd_len) < 0)
        return FLASH_ERROR;

    uint8_t ack;
    int rcv_len = serial_wait_packet(port, &ack, 1, 50 * TIMEOUT);
    if (rcv_len <= 0)
        return FLASH_ERROR;

    uint8_t *buf = malloc(size);
    if (!buf)
        return FLASH_ERROR;

    size_t pos = 0;
    while (pos < size)
    {
        uint8_t tmp[0x800];
        int data_len = flash_recv_block(port, addr + pos, tmp, sizeof(tmp));
        if (data_len < 0)
        {
            free(buf);
            return FLASH_ERROR;
        }

        if (pos + data_len > size)
            data_len = size - pos;

        memcpy(buf + pos, tmp, data_len);
        pos += data_len;

        if (pos < size)
        {
            uint8_t ack = SERIAL_ACK;
            if (serial_write(port, &ack, 1) < 0)
            {
                free(buf);
                return FLASH_ERROR;
            }
        }
    }

    char fw_id[128];
    if (scan_fw_version(buf, size, fw_id, sizeof(fw_id)) == 0)
    {
        printf("\nFW Version: %s\n", fw_id);
        size_t len = strlen(fw_id);
        if (len >= sizeof(phone->fw_version))
            len = sizeof(phone->fw_version) - 1;

        memcpy(phone->fw_version, fw_id, len);
        phone->fw_version[len] = '\0';
        free(buf);
        return FLASH_OK;
    }

    free(buf);
    return FLASH_ERROR;
}

int flash_detect_fw_version(struct sp_port *port, struct phone_info *phone)
{
    if (phone->chip_id == PNX5230)
    {
        // --- primary scan
        if (flash_scan_fw_version(port, phone, 0x216E0000, 3 * BLOCK_SIZE) == 0)
            return FLASH_OK;

        // --- fallback for Z310 fw
        return flash_scan_fw_version(port, phone, 0x213FC000, BLOCK_SIZE);
    }
    else if (phone->chip_id == DB2010_2)
    {
        return flash_scan_fw_version(port, phone, 0x44880000, 16 * BLOCK_SIZE);
    }
    else if (phone->chip_id == DB2020)
    {
        return flash_scan_fw_version(port, phone, 0x45B10000, 8 * BLOCK_SIZE);
    }

    printf("Unsupported chip id: %08X\n", phone->chip_id);
    return FLASH_ERROR;
}

int flash_read(struct sp_port *port, struct phone_info *phone,
               uint32_t memaddr, int size)
{
    // Build request payload [start, end]
    uint32_t addr_range[2] = {memaddr, memaddr + size};

    uint8_t cmd_buf[64];
    int cmd_len = cmd_encode_binary_packet(0x32,
                                           (uint8_t *)addr_range,
                                           sizeof(addr_range),
                                           cmd_buf);
    if (cmd_len <= 0)
        return FLASH_ERROR;

    if (serial_send_packetdata_ack(port, cmd_buf, cmd_len) < 0)
        return FLASH_ERROR;

    uint8_t ack;
    int rcv_len = serial_wait_packet(port, &ack, 1, 50 * TIMEOUT);
    if (rcv_len <= 0)
        return FLASH_ERROR;

    char rawfile[1024];
    snprintf(rawfile, sizeof(rawfile),
             "./backup/flashdump_%s_%08X_%08X.bin",
             phone->otp_imei,
             addr_range[0], size);

    FILE *out = fopen(rawfile, "wb");
    if (!out)
    {
        perror("open output");
        return FLASH_ERROR;
    }

    printf("\nreading raw: %s\n", rawfile);
    int total_blocks = (size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    printf("reading %d blocks\n", total_blocks);

    int pos = 0;
    while (pos < size)
    {
        uint8_t buf[0x800];
        int data_len = flash_recv_block(port, memaddr + pos, buf, sizeof(buf));
        if (data_len < 0)
        {
            fclose(out);
            return FLASH_ERROR;
        }

        if (pos + data_len > size)
            data_len = size - pos; // truncate last block

        fwrite(buf, 1, data_len, out);
        pos += data_len;

        // only ACK if not last
        if (pos < size)
        {
            uint8_t ack = SERIAL_ACK;
            if (serial_write(port, &ack, 1) < 0)
            {
                fclose(out);
                return FLASH_ERROR;
            }
        }

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

    // --- optional babe conversion ---
    if (phone->save_as_babe)
    {
        char babefile[1024];
        snprintf(babefile, sizeof(babefile),
                 "./backup/flashdump_%s_%08X_%08X.ssw",
                 phone->otp_imei,
                 addr_range[0], size);

        printf("converting to babe: %s\n", babefile);
        if (raw_to_babe_file(rawfile, babefile, memaddr) != 0)
        {
            fprintf(stderr, "Error: failed to convert raw to babe\n");
            return FLASH_ERROR;
        }
        remove(rawfile);
    }

    return FLASH_OK;
}