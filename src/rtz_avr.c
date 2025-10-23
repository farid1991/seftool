#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <libserialport.h>

#include "common.h"
#include "cmd.h"
#include "connection.h"
#include "serial.h"
#include "rtz_avr.h"
#include "rtz.h"

uint8_t avr_get_speed_val(int speed)
{
    if (speed >= 921600)
        return AVRCOMSPEED921600;
    if (speed >= 460800)
        return AVRCOMSPEED460800;
    if (speed >= 230400)
        return AVRCOMSPEED230400;
    if (speed >= 115200)
        return AVRCOMSPEED115200;
    return 0x10;
}

int rtz_avr_set_com_speed(struct sp_port *port, int baudrate)
{
    printf("Set avr port speed to %d...", baudrate);
    send_byte(port, STCMD_18_SETAVRCOMSPEED);
    send_byte(port, avr_get_speed_val(baudrate));

    if (serial_set_baudrate(port, baudrate) != 0)
        return -1;

    send_byte(port, 0);

    if (wait_for_byte(port, '>') != 0)
        return -1;

    printf("OK\n");
    return 0;
}

int rtz_avr_get_otp_imei(struct sp_port *port, struct phone_info *phone)
{
    send_byte(port, 'I');

    uint8_t resp[15] = {0};
    int rcv_len = recv_block(port, resp, sizeof(resp));
    if (rcv_len <= 0)
        return -1;

    memcpy(phone->otp_imei, resp, sizeof(resp));
    phone->otp_imei[sizeof(resp)] = '\0';

    printf("OTP IMEI: %s\n", phone->otp_imei);

    // --- Read response 0x0D 0x0A 0x3E
    if (wait_for_byte(port, '>') != 0)
        return -1;

    return 0;
}

int rtz_avr_get_gdfs_imei(struct sp_port *port, struct phone_info *phone)
{
    send_byte(port, 'G');

    uint8_t resp[15] = {0};
    int rcv_len = recv_block(port, resp, sizeof(resp));
    if (rcv_len <= 0)
        return -1;

    memcpy(phone->gdfs_imei, resp, sizeof(resp));
    phone->gdfs_imei[sizeof(resp)] = '\0';

    printf("GDFS IMEI: %s\n", phone->gdfs_imei);

    // --- Read response 0x0D 0x0A 0x3E
    if (wait_for_byte(port, '>') != 0)
        return -1;

    return 0;
}

int rtz_get_flash_id(struct sp_port *port, struct phone_info *phone)
{
    // Get Flash data
    uint8_t cmd_buf[8];
    int cmd_len = cmd_encode_binary_packet(0x0D, NULL, 0, cmd_buf);
    if (cmd_len <= 0)
        return -1;

    // --- send command
    if (serial_send_packetdata_ack(port, cmd_buf, cmd_len) < 0)
        return -1;

    if (serial_wait_ack(port, 10 * TIMEOUT) != 0)
        return -1;

    uint8_t resp[16];
    int rcv_len = serial_read(port, resp, sizeof(resp), 10 * TIMEOUT);
    if (rcv_len <= 0)
        return -1;

    struct packetdata_t repl;
    if (cmd_decode_packet(resp, rcv_len, &repl) != 0)
        return -1;

    if (repl.cmd != 0x0A && repl.length != 2)
        return -1;

    phone->flash_id = (repl.data[0] << 8) | repl.data[1];
    printf("AVR FlashID: 0x%x (%s)\n", phone->flash_id, get_flash_vendor(phone->flash_id));

    return 0;
}

int rtz_avr_send_bootloader(struct sp_port *port)
{
    printf("Sending avr-bootloader... ");

    char loader_name[] = "./loader/rtz/avr_bootloader.bin";

    size_t ldr_size;
    uint8_t *ldr = load_file(loader_name, &ldr_size);

    uint16_t loadaddr = 0x800;
    uint16_t endaddr = loadaddr + (ldr_size / 2);

    uint8_t header[4] = {
        loadaddr & 0xFF,
        (loadaddr >> 8) & 0xFF,
        endaddr & 0xFF,
        (endaddr >> 8) & 0xFF,
    };

    // Send 4-byte header
    send_block(port, header, sizeof(header));

    // Send data block
    send_chunk(port, ldr, ldr_size);

    free(ldr);

    if (wait_for_byte(port, '>') != 0)
        return -1;

    printf("OK\n");

    return 0;
}

#define AVRFLASHBLOCKS_MAXNUM 16
static AVRFlashEBInfo avrflashblocks[AVRFLASHBLOCKS_MAXNUM];
static int avrflashblocksnum;

const char *get_flash_name(uint16_t flashid)
{
    switch (flashid)
    {
    case 0x01:
        return "AMD";
    case 0x04:
        return "Fujitsu";
    case 0x20:
        return "STMicro";
    case 0x89:
        return "Intel";
    case 0x1F:
        return "Atmel";
    case 0x98:
        return "Toshiba";
    case 0xBF:
        return "SST";
    default:
        return "unknown";
    }
}

int rtz_avr_get_flash_info(struct sp_port *port)
{
    CFI cfi;

    printf("AVR flash search\n");
    avrflashblocksnum = 0;

    // Send command to get flash CFI info
    send_byte(port, STCMD_19_GETCFIINFO);

    for (;;)
    {
        int i = recv_byte(port);
        if (i == 'P')
        {
            int byte_hi = recv_byte(port);
            if (byte_hi < 0)
            {
                fprintf(stderr, "Unexpected end of data\n");
                return -2;
            }

            uint32_t curaddr = (uint32_t)byte_hi << 16;
            printf("  Found flash at 0x%06X\n", curaddr);

            if (!recv_block(port, &cfi, sizeof(cfi)))
            {
                fprintf(stderr, "Can't get CFI block\n");
                return -3;
            }

            int crc = recv_byte(port);
            if (crc < 0)
            {
                fprintf(stderr, "Missing CRC byte\n");
                return -4;
            }

            uint8_t calc_crc = simple_crc_add((uint8_t *)&cfi, sizeof(cfi));
            if (calc_crc != crc)
            {
                fprintf(stderr, "Warning: CFI CRC mismatch (expected 0x%02X, got 0x%02X)\n", crc, calc_crc);
            }

            printf("    Flash MfrId: 0x%X (%s), DevId: 0x%X\n",
                   cfi.MfrId, get_flash_name(cfi.MfrId), cfi.DevId);

            printf("    VCC min  %d.%d,  VCC max  %d.%d\n",
                   cfi.VCCLogicSupply_minimum_wrer_voltage >> 4,
                   cfi.VCCLogicSupply_minimum_wrer_voltage & 0xF,
                   cfi.VCCLogicSupply_maximum_wrer_voltage >> 4,
                   cfi.VCCLogicSupply_maximum_wrer_voltage & 0xF);

            printf("    DeviceSize: %u bytes\n", 1u << cfi.DeviceSize);
            printf("    NumberOfEraseBlockRegions: %u\n", cfi.NumberOfEraseBlockRegions);

            for (int r = 0; r < cfi.NumberOfEraseBlockRegions; r++)
            {
                if (avrflashblocksnum >= AVRFLASHBLOCKS_MAXNUM)
                {
                    fprintf(stderr, "Too many erase regions\n");
                    break;
                }

                AVRFlashEBInfo *blk = &avrflashblocks[avrflashblocksnum];
                blk->address = curaddr;

                uint16_t blocks_minus1 = get_half(&cfi.eraseblockregions[r * 4]);
                uint16_t block_size = get_half(&cfi.eraseblockregions[r * 4 + 2]);

                blk->blocks = blocks_minus1 + 1;
                blk->blocksize = block_size ? block_size * 256 : 128;

                uint32_t region_size = blk->blocks * blk->blocksize;

                printf("          0x%08X - 0x%08X : %u blocks x %u bytes\n",
                       blk->address, blk->address + region_size - 1,
                       blk->blocks, blk->blocksize);

                curaddr += region_size;

                // Detect algorithm
                switch (cfi.MfrId)
                {
                case 0x89:
                    blk->algorithm = 2;
                    break; // Intel
                case 0x27:
                    blk->algorithm = 2;
                    break;
                case 0x01:
                    blk->algorithm = 1;
                    break; // AMD
                case 0x04:
                    blk->algorithm = 1;
                    break; // Fujitsu
                case 0x20:
                    blk->algorithm = ((cfi.DevId >> 8) == 0x88) ? 2 : 1;
                    break;
                default:
                    blk->algorithm = 0;
                    fprintf(stderr, "Unknown flash algorithm\n");
                    break;
                }

                avrflashblocksnum++;
            }
            send_byte(port, 0x00); // continue
        }
        else if (i == '>')
        {
            break;
        }
        else if (i < 0)
        {
            fprintf(stderr, "Communication error\n");
            return -5;
        }
        else
        {
            fprintf(stderr, "Unexpected reply: 0x%02X\n", i);
            return -6;
        }
    }

    return 0;
}

static int rtz_avr_eraseblock__(struct sp_port *port, uint32_t addr, uint8_t cmd)
{
    send_byte(port, cmd);
    send_mediumword(port, addr);

    if (recv_byte(port) != 0 || recv_byte(port) != '>')
        return 0;

    return 1;
}

static int rtz_avr_eraseblock(struct sp_port *port, uint32_t addr, int algo)
{
    switch (algo)
    {
    case 1: // AMD-type
        if (!rtz_avr_eraseblock__(port, addr, STCMD_A_ERASEAVR_FLASHTYPE1_STEP1))
            return -1;
        if (!rtz_avr_eraseblock__(port, addr, STCMD_B_ERASEAVR_FLASHTYPE1_STEP2))
            return -2;
        break;

    case 2: // Intel-type
        if (!rtz_avr_eraseblock__(port, addr, STCMD_E_ERASEAVR_FLASHTYPE2_STEP1))
            return -3;
        if (!rtz_avr_eraseblock__(port, addr, STCMD_F_ERASEAVR_FLASHTYPE2_STEP2))
            return -4;
        break;

    default:
        return -5;
    }

    return 0;
}

static int rtz_avr_writeblock(struct sp_port *port, uint32_t avraddr, int avrlen,
                              const uint8_t *from, int algo)
{
    uint8_t crc;

    if (algo == 2)
        send_byte(port, STCMD_10_FLASHAVR_TYPE2);
    else if (algo == 1)
        send_byte(port, STCMD_04_FLASHAVR_TYPE1);
    else
    {
        fprintf(stderr, "Unknown algo: %d\n", algo);
        return -1;
    }

    send_mediumword(port, avraddr);
    send_half(port, (avrlen + 1) / 2);
    send_chunk(port, from, avrlen);

    crc = simple_crc_add(from, avrlen);
    if (avrlen & 1)
    {
        send_byte(port, 0xFF);
        crc += 0xFF;
    }
    send_byte(port, crc);

    if (recv_byte(port) != 0 || recv_byte(port) != '>')
        return -1;

    return 0;
}

int rtz_avr_write(struct sp_port *port, uint32_t avraddr, uint32_t avrlen, const uint8_t *from)
{
    uint32_t nexteraseaddr = avraddr;
    uint32_t total = avrlen;

    printf("[ERASE] %08X\n[WRITE] %08X", nexteraseaddr, avraddr);
    fflush(stdout);

    while (avrlen > 0)
    {
        int fls = 0;
        while (fls < avrflashblocksnum &&
               (avrflashblocks[fls].address +
                avrflashblocks[fls].blocksize * avrflashblocks[fls].blocks) <= avraddr)
        {
            fls++;
        }

        if (fls >= avrflashblocksnum)
        {
            printf("\nError: No flash at 0x%08X\n", avraddr);
            return -1;
        }

        if (!avrflashblocks[fls].algorithm)
        {
            printf("\nError: Unknown flash algorithm at 0x%08X\n", avraddr);
            return -1;
        }

        if (avraddr >= nexteraseaddr)
        {
            // move cursor to first line and update
            printf("\033[1F\r[ERASE] 0x%08X\n", avraddr);
            fflush(stdout);

            if (rtz_avr_eraseblock(port, avraddr, avrflashblocks[fls].algorithm))
            {
                printf("\nErase failed at 0x%08X\n", avraddr);
                return -1;
            }

            nexteraseaddr = avrflashblocks[fls].blocksize + avraddr;
        }

        // move cursor to second line and update
        printf("\033[1E\r[WRITE] 0x%08X", avraddr);
        fflush(stdout);

        uint32_t wrsize = avrlen;
        if (wrsize > avrflashblocks[fls].blocksize)
            wrsize = avrflashblocks[fls].blocksize;

        if (rtz_avr_writeblock(port, avraddr, wrsize, from, avrflashblocks[fls].algorithm))
        {
            printf("\nWrite failed at 0x%08X\n", avraddr);
            return -1;
        }

        avraddr += wrsize;
        from += wrsize;
        avrlen -= wrsize;
    }

    printf("\nDone (%u bytes)\n", total);
    return 0;
}

int rtz_avr_flash_bin(struct sp_port *port, uint32_t avrbase, const char *firmware)
{
    if (!avrbase)
        avrbase = 0x800000;

    size_t fsize;
    uint8_t *fw = load_file(firmware, &fsize);
    if (!fw)
    {
        fprintf(stderr, "Error: failed to load firmware %s\n", firmware);
        return -1;
    }
    printf("\nFlashing: %s\n", firmware);

    int res = rtz_avr_write(port, avrbase, fsize, fw);
    free(fw);

    if (res == 0)
        printf("\n[OK] AVR flash completed successfully\n");
    else
        fprintf(stderr, "\n[ERROR] AVR flash failed (code %d)\n", res);

    return res;
}

int rtz_avr_activate_loader(struct sp_port *port, struct phone_info *phone)
{
    send_byte(port, 'Q');

    uint8_t hello_buf[64];
    int rcv_len = serial_wait_packet(port, hello_buf, sizeof(hello_buf), 10 * TIMEOUT);
    if (rcv_len <= 0)
        return -1;

    struct packetdata_t repl;
    if (cmd_decode_packet(hello_buf, rcv_len, &repl) != 0)
        return -1;

    char loader_hello[64];
    if (repl.length >= sizeof(loader_hello))
        return -1;

    memcpy(loader_hello, repl.data, repl.length);
    loader_hello[repl.length] = '\0';

    printf("LDR: %s\n", loader_hello);

    if (rtz_get_flash_id(port, phone) != 0)
        return -1;

    return 0;
}
