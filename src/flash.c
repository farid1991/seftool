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
#include "vkp.h"

#define FLASH_OK 0
#define FLASH_ERROR -1

// ------------- Write to Flash -------------

int flash_babe(struct sp_port *port, uint8_t *addr, size_t size, int flashfull)
{
    struct babehdr_t *hdr = (struct babehdr_t *)addr;
    int fileformatver = hdr->ver;
    size_t hashsize = fileformatver >= 4 ? 20 : 1;
    int blocks = hdr->payloadsize1;
    size_t hdrsize = (fileformatver <= 2) ? 0x480 : blocks * hashsize + 0x380;

    // --- send header ---
    size_t curpos = 0;
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
        int rcv_len = serial_wait_packet(port, resp, sizeof(resp), TIMEOUT);
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

        if (serial_wait_ack(port, 50) < 0)
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
            if (serial_wait_ack(port, 50) < 0)
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

    printf("\n%d blocks flashed ok\n", blocks);

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

uint8_t *flash_convert_raw_to_babe(uint8_t *raw, size_t size, uint32_t raw_addr, size_t *babe_size_out)
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
        *((uint32_t *)(babe + pos)) = raw_addr;
        pos += 4;
        *((uint32_t *)(babe + pos)) = bsize;
        pos += 4;
        memcpy(babe + pos, rawp, bsize);
        pos += bsize;

        rawp += bsize;
        left -= bsize;
        raw_addr += bsize;
    }

    if (babe_size_out)
        *babe_size_out = babesize;

    return babe;
}

int flash_cnv_babe_to_raw_file(const char *babe_filename, const char *raw_filename)
{
    FILE *fin = fopen(babe_filename, "rb");
    if (!fin)
    {
        fprintf(stderr, "file (%s) does not exist!\n", babe_filename);
        return -1;
    }

    fseek(fin, 0, SEEK_END);
    size_t fsize = ftell(fin);
    fseek(fin, 0, SEEK_SET);

    uint8_t *buf = malloc(fsize);
    if (!buf)
    {
        fclose(fin);
        return -1;
    }

    if (fread(buf, 1, fsize, fin) != fsize)
    {
        fprintf(stderr, "Failed to read babe file (%s)\n", babe_filename);
        free(buf);
        fclose(fin);
        return -1;
    }
    fclose(fin);

    struct babehdr_t *hdr = (struct babehdr_t *)buf;
    if (hdr->sig != 0xBEBA)
    {
        fprintf(stderr, "Error: not a BABE file\n");
        free(buf);
        return -1;
    }

    uint8_t ver = hdr->ver;
    uint32_t blocks = hdr->payloadsize1;
    uint32_t bsize = hdr->payloadsize2;

    printf("BABE version %u, blocks=%u, blocksize=0x%X\n", ver, blocks, bsize);

    // decide hash mode by "ver"
    size_t hptr = sizeof(struct babehdr_t);
    size_t dptr = 0;
    if (ver == 2)
        dptr = hptr + 0x100; // static hash
    else if (ver == 3)
        dptr = blocks + hptr; // dynamic hash
    else if (ver == 4)
        dptr = (size_t)blocks * 20 + hptr; // full hash
    else
    {
        fprintf(stderr, "Error: Unknown hash type %u\n", ver);
        free(buf);
        return -1;
    }

    FILE *out = fopen(raw_filename, "wb");
    if (!out)
    {
        fprintf(stderr, "Failed to open raw file (%s)\n", raw_filename);
        free(buf);
        return -1;
    }

    uint32_t offset = get_word(buf + dptr);
    size_t curptr = dptr;

    printf("Converting...\n");

    for (uint32_t i = 0; i < blocks; i++)
    {
        uint32_t cur_bsize = get_word(buf + curptr + 4);
        uint32_t cur_bdata = get_word(buf + curptr) - offset;

        if (cur_bdata > BLOCK_SIZE)
            cur_bdata = 0;

        offset += cur_bdata;

        if (curptr + 8 + cur_bsize > fsize)
            break;

        if (fwrite(buf + curptr + 8, 1, cur_bsize, out) != cur_bsize)
        {
            fclose(out);
            free(buf);
            fprintf(stderr, "Failed to write raw file (%s)\n", babe_filename);
            return -1;
        }

        offset += cur_bsize;
        curptr += cur_bsize + 8;
    }

    fclose(out);
    free(buf);
    printf("Converted %s -> %s\n", babe_filename, raw_filename);
    return 0;
}

int flash_cnv_raw_to_babe_file(const char *raw_filename, const char *babe_filename, uint32_t raw_addr)
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
    uint8_t *babe = flash_convert_raw_to_babe(raw, size, raw_addr, &babe_size);
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

int flash_raw(struct sp_port *port, const char *filename, uint32_t raw_addr)
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
    uint8_t *babe = flash_convert_raw_to_babe(raw, size, raw_addr, &babesize);
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

uint8_t *flash_read_raw(struct sp_port *port, uint32_t addr, size_t size)
{
    uint32_t addr_range[2] = {addr, addr + size};

    uint8_t cmd_buf[32];
    int cmd_len = cmd_encode_binary_packet(0x32,
                                           (uint8_t *)addr_range,
                                           sizeof(addr_range),
                                           cmd_buf);
    if (cmd_len <= 0)
        return NULL;

    if (serial_send_packetdata_ack(port, cmd_buf, cmd_len) < 0)
        return NULL;

    uint8_t ack;
    int rcv_len = serial_wait_packet(port, &ack, 1, 50 * TIMEOUT);
    if (rcv_len <= 0)
        return NULL;

    uint8_t *buf = malloc(size);
    if (!buf)
        return NULL;

    size_t pos = 0;
    while (pos < size)
    {
        uint8_t tmp[0x800];
        int data_len = flash_recv_block(port, addr + pos, tmp, sizeof(tmp));
        if (data_len < 0)
        {
            free(buf);
            return NULL;
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
                return NULL;
            }
        }
    }

    return buf; // caller must free()
}

int flash_scan_fw_version(struct sp_port *port, struct phone_info *phone,
                          uint32_t addr, size_t size)
{
    uint8_t *buf = flash_read_raw(port, addr, size);
    if (!buf)
        return FLASH_ERROR;

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
               uint32_t addr, size_t size)
{
    char rawfile[1024];
    snprintf(rawfile, sizeof(rawfile),
             "./backup/flashdump_%s_%08X_%08zX.bin",
             phone->otp_imei,
             addr, size);

    FILE *out = fopen(rawfile, "wb");
    if (!out)
    {
        perror("open output");
        return FLASH_ERROR;
    }

    printf("\nreading raw: %s\n", rawfile);
    size_t total_blocks = (size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    printf("reading %zu blocks\n", total_blocks);

    size_t pos = 0;
    while (pos < size)
    {
        size_t chunk = (size - pos < BLOCK_SIZE) ? (size - pos) : BLOCK_SIZE;
        uint8_t *buf = flash_read_raw(port, addr + pos, chunk);
        if (!buf)
        {
            fclose(out);
            return FLASH_ERROR;
        }

        fwrite(buf, 1, chunk, out);
        free(buf);

        pos += chunk;

        size_t current_block = pos / BLOCK_SIZE;
        printf("\rreading block: %zu/%zu (addr 0x%08zX size 0x%zX)",
               current_block, total_blocks, addr + pos, chunk);
        fflush(stdout);
    }

    printf("\n\n");
    fclose(out);

    // --- optional babe conversion ---
    if (phone->save_as_babe)
    {
        char babefile[1024];
        snprintf(babefile, sizeof(babefile),
                 "./backup/flashdump_%s_%08X_%08zX.ssw",
                 phone->otp_imei,
                 addr, size);

        printf("converting to babe: %s\n", babefile);
        if (flash_cnv_raw_to_babe_file(rawfile, babefile, addr) != 0)
        {
            fprintf(stderr, "Error: failed to convert raw to babe\n");
            return FLASH_ERROR;
        }
        remove(rawfile);
    }

    return FLASH_OK;
}

static user_choice_t ask_user_choice(const char *prompt,
                                     const char *options)
{
    // Example: options = "[u]ninstall / [s]kip / [a]bort"
    char buf[16];
    printf("%s %s ? ", prompt, options);
    fflush(stdout);

    if (!fgets(buf, sizeof(buf), stdin))
        return CHOICE_ABORT;

    switch (buf[0])
    {
    case 'u':
    case 'U':
        return CHOICE_UNINSTALL;
    case 's':
    case 'S':
        return CHOICE_SKIP;
    case 'a':
    case 'A':
        return CHOICE_ABORT;
    case 'c':
    case 'C':
    case 'y':
    case 'Y':
        return CHOICE_CONTINUE;
    default:
        return CHOICE_ABORT;
    }
}

static int cmp_u32(const void *a, const void *b)
{
    uint32_t va = *(const uint32_t *)a;
    uint32_t vb = *(const uint32_t *)b;

    if (va < vb)
        return -1;
    if (va > vb)
        return 1;
    return 0;
}

int flash_vkp(struct sp_port *port, const char *filename, vkp_patch_t *patch,
              int remove_flag, size_t flashblocksize)
{
    if (!patch || patch->patch.count == 0)
    {
        fprintf(stderr, "empty patch\n");
        return FLASH_VKP_ERR;
    }
    if (patch->errorline)
    {
        fprintf(stderr, "%s: error in line %d\n", filename, patch->errorline);
        return FLASH_VKP_ERR;
    }
    if (flashblocksize == 0)
    {
        fprintf(stderr, "unknown flash chip\n");
        return FLASH_VKP_ERR;
    }

    // 1. Collect blocks
    uint32_t blocks[MAX_BLOCKS];
    size_t nblocks = vkp_collect_unique_blocks(patch, flashblocksize, blocks, MAX_BLOCKS);
    if (nblocks == 0)
    {
        fprintf(stderr, "no blocks to patch\n");
        return FLASH_VKP_ERR;
    }
    qsort(blocks, nblocks, sizeof(uint32_t), cmp_u32);

    size_t chunks_per_flashblock = flashblocksize / BLOCK_SIZE;
    size_t realblocks = nblocks * chunks_per_flashblock;

    // 2. Allocate babe
    size_t babe_size = sizeof(struct babehdr_t) + realblocks /*hash*/ +
                       realblocks * (8 + BLOCK_SIZE);
    uint8_t *babe = calloc(1, babe_size);
    if (!babe)
    {
        fprintf(stderr, "malloc failed\n");
        return FLASH_VKP_ERR;
    }

    struct babehdr_t *hdr = (struct babehdr_t *)babe;
    hdr->sig = 0xBEBA;
    hdr->ver = 3;
    hdr->payloadsize1 = (uint32_t)realblocks;

    size_t pos = sizeof(struct babehdr_t) + realblocks; // skip header + hash
    int block_index = 0;
    int total_blocks = (int)realblocks;

    for (size_t b = 0; b < nblocks; b++)
    {
        uint32_t base = blocks[b];
        for (size_t off = 0; off < flashblocksize; off += BLOCK_SIZE)
        {
            uint32_t chunk_addr = base + (uint32_t)off;

            ((uint32_t *)(babe + pos))[0] = chunk_addr;
            ((uint32_t *)(babe + pos))[1] = BLOCK_SIZE;
            pos += 8;

            uint8_t *dst = babe + pos;
            uint8_t *raw = flash_read_raw(port, chunk_addr, BLOCK_SIZE);
            if (!raw)
            {
                fprintf(stderr, "\nread failed at 0x%08X\n", chunk_addr);
                free(babe);
                return FLASH_VKP_ERR;
            }
            memcpy(dst, raw, BLOCK_SIZE);
            free(raw);

            pos += BLOCK_SIZE;

            block_index++;
            printf("\rReading block %d/%d (addr %08X size %08X) ",
                   block_index, total_blocks, chunk_addr, BLOCK_SIZE);
            fflush(stdout);
        }
    }
    printf("\n");

    // 3. Scan patches for mismatch
    int unmatched = 0, contrmatched = 0;
    for (size_t pi = 0; pi < patch->patch.count; pi++)
    {
        vkp_line_t *ln = &patch->patch.lines[pi];
        uint32_t addr = ln->addr;

        size_t p = sizeof(struct babehdr_t) + realblocks;
        while (1)
        {
            uint32_t curaddr = ((uint32_t *)(babe + p))[0];
            uint32_t cursize = ((uint32_t *)(babe + p))[1];
            if (addr < curaddr + cursize)
            {
                uint8_t actual = babe[p + 8 + (addr - curaddr)];
                if (actual != ln->data[remove_flag])
                    unmatched++;
                if (actual == ln->data[remove_flag ^ 1])
                    contrmatched++;
                break;
            }
            p += 8 + BLOCK_SIZE;
        }
    }

    // patch already installed
    if (unmatched && contrmatched == (int)patch->patch.count)
    {
        user_choice_t choice = ask_user_choice(filename, "[u]ninstall / [s]kip / [a]bort");

        if (choice == CHOICE_UNINSTALL)
        {
            remove_flag ^= 1;
            unmatched = 0;
        }
        else if (choice == CHOICE_SKIP)
        {
            free(babe);
            printf("skipping %s\n", filename);
            return FLASH_VKP_SKIP;
        }
        else // abort
        {
            free(babe);
            return FLASH_VKP_ERR;
        }
    }

    // patch mismatch
    if (unmatched)
    {
        char prompt[128];
        snprintf(prompt, sizeof(prompt),
                 "%d/%zu mismatch", unmatched, patch->patch.count);

        user_choice_t choice = ask_user_choice(prompt, "[c]ontinue / [s]kip / [a]bort");

        if (choice == CHOICE_SKIP)
        {
            free(babe);
            return FLASH_VKP_SKIP; // skip this patch
        }
        else if (choice == CHOICE_ABORT)
        {
            free(babe);
            return FLASH_VKP_ERR;
        }
        // CHOICE_CONTINUE → proceed
    }

    printf("making a patched babe\n");

    // 4. Apply patches
    for (size_t pi = 0; pi < patch->patch.count; pi++)
    {
        vkp_line_t *ln = &patch->patch.lines[pi];
        uint32_t addr = ln->addr;

        size_t p = sizeof(struct babehdr_t) + realblocks;
        while (1)
        {
            uint32_t curaddr = ((uint32_t *)(babe + p))[0];
            uint32_t cursize = ((uint32_t *)(babe + p))[1];
            if (addr < curaddr + cursize)
            {
                babe[p + 8 + (addr - curaddr)] = ln->data[remove_flag ^ 1];
                break;
            }
            p += 8 + BLOCK_SIZE;
        }
    }

    // 5. Flash
    int rc = flash_babe(port, babe, pos, 1);
    free(babe);
    return rc == 0 ? FLASH_VKP_OK : FLASH_VKP_ERR;
}
