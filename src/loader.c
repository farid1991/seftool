#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <libserialport.h>

#include "babe.h"
#include "common.h"
#include "cmd.h"
#include "loader.h"
#include "gdfs.h"
#include "serial.h"

int wait_e3_answer(struct sp_port *port, const char *expected, int timeout_ms, int skiperrors)
{
    uint8_t buf[3];
    int got = 0;

    while (got < 3)
    {
        int rcv_len = sp_blocking_read(port, buf + got, 3 - got, timeout_ms);
        if (rcv_len <= 0)
        {
            fprintf(stderr, "Read failed (rcv_len=%d)\n", rcv_len);
            return -1;
        }
        got += rcv_len;
    }

    if (memcmp(buf, expected, 3) != 0)
    {
        if (skiperrors == 1)
            return 0;
        fprintf(stderr, "[wait_e3_answer] Unexpected reply: %.*s (expected %s)\n", 3, buf, expected);
        return -1;
    }

    return 0;
}

int loader_get_hello(struct packetdata_t *packet)
{
    char loader_hello[256];
    if (packet->length >= sizeof(loader_hello))
        return -1;

    memcpy(loader_hello, packet->data, packet->length);
    loader_hello[packet->length] = '\0';

    printf("LDR: %s\n", loader_hello);

    loader_type = LDR_UNKNOWN;

    if (strstr(loader_hello, "CS_LOADER") || strstr(loader_hello, "CSLOADER"))
    {
        // printf("This is a CHIPSELECT loader\n");
        loader_type = LDR_CHIPSELECT;
    }
    else if (strstr(loader_hello, "FILESYSTEMLOADER") || strstr(loader_hello, "FILE_SYSTEM_LOADER"))
    {
        // printf("This is a FILESYSTEM loader\n");
        loader_type = LDR_CHIPSELECT;
    }
    else if (strstr(loader_hello, "PRODUCTION_ID") || strstr(loader_hello, "PRODUCTIONID"))
    {
        // printf("This is a PRODUCTION_ID loader\n");
        loader_type = LDR_PRODUCT_ID;
    }
    else if (strstr(loader_hello, "CERTLOADER"))
    {
        // printf("This is a CERTIFICATE loader\n");
        loader_type = LDR_CERT;
    }
    else if (strstr(loader_hello, "FLASHLOADER"))
    {
        // printf("This is a FLASH loader\n");
        loader_type = LDR_FLASH;
    }
    else if (strstr(loader_hello, "MEM_PATCHER"))
    {
        // printf("This is a MEM_PATCHER loader\n");
        loader_type = LDR_FLASH;
    }
    else if (strstr(loader_hello, "patched"))
    {
        // printf("This is a patched loader\n");
        loader_type = LDR_FLASH;
    }
    else
    {
        // printf("%s\nThis is an unknown loader type\n", loader_hello);
        loader_type = LDR_UNKNOWN;
    }

    if (strstr(loader_hello, "SETOOL"))
        printf("Let's say thanks to the_laser =)\n");

    if (strstr(loader_hello, "den_po"))
        printf("Let's say thanks to den_po =)\n");

    return 0;
}

int loader_send_binary_cmd3e(struct sp_port *port, const char *loader_name)
{
    FILE *fb = fopen(loader_name, "rb");
    if (!fb)
    {
        fprintf(stderr, "Loader file (%s) does not exist!\n", loader_name);
        return -1;
    }

    // Read entire loader into memory
    fseek(fb, 0, SEEK_END);
    uint32_t fsize = ftell(fb);
    fseek(fb, 0, SEEK_SET);

    uint8_t *buffer = malloc(fsize);
    if (!buffer)
    {
        fclose(fb);
        fprintf(stderr, "No memory for loader (%d bytes)\n", fsize);
        return -1;
    }

    fread(buffer, 1, fsize, fb);
    fclose(fb);

    uint8_t cmd_buf[0x800];
    int cmd_len = cmd_encode_binary_packet(0x3E, buffer, fsize, cmd_buf);
    if (cmd_len <= 0)
    {
        free(buffer);
        return -1;
    }
    free(buffer);

    // --- send cmd3e command
    if (serial_send_packetdata_ack(port, cmd_buf, cmd_len) < 0)
        return -1;

    if (serial_wait_ack(port, TIMEOUT) < 0)
        return -1;

    // --- Read hello response from loader =)
    uint8_t hello_buf[128];
    int rcv_len = serial_wait_packet(port, hello_buf, sizeof(hello_buf), 5 * TIMEOUT);
    if (rcv_len <= 0)
        return -1;

    struct packetdata_t repl;
    if (hello_buf[0] == SERIAL_HDR89)
    {
        if (cmd_decode_packet_noack(hello_buf, rcv_len, &repl) != 0)
            return -1;
    }
    else if (hello_buf[0] == SERIAL_ACK && hello_buf[1] == SERIAL_HDR89)
    {
        if (cmd_decode_packet_ack(hello_buf, rcv_len, &repl) != 0)
            return -1;
    }
    else if (hello_buf[0] == 0xFC && hello_buf[1] == 0xFF) // CMD3E DB2000 DB2010 from SETOOL
    {
        printf("Break CMD3E\n");
        return 0;
    }
    else
    {
        fprintf(stderr, "[CMD3E] Unexpected reply %X %X\n", hello_buf[0], hello_buf[1]);
        return -1;
    }

    loader_get_hello(&repl);

    return 0;
}

int loader_send_unsigned_ldr(struct sp_port *port, const char *loader_name, uint32_t ram_addr)
{
    FILE *fh = fopen(loader_name, "rb");
    if (!fh)
    {
        fprintf(stderr, "Loader file (%s) does not exist!\n", loader_name);
        return -1;
    }

    // printf("Sending %s ... \n", loader_name);

    // Read entire loader into memory
    fseek(fh, 0, SEEK_END);
    uint32_t fsize = ftell(fh);
    fseek(fh, 0, SEEK_SET);

    uint8_t *buffer = malloc(fsize);
    if (!buffer)
    {
        fclose(fh);
        fprintf(stderr, "No memory for loader (%d bytes)\n", fsize);
        return -1;
    }

    fread(buffer, 1, fsize, fh);
    fclose(fh);

    // --- Build ram_addr packet (little endian)
    uint8_t addr_packet[4];
    addr_packet[0] = (uint8_t)(ram_addr & 0xFF);
    addr_packet[1] = (uint8_t)((ram_addr >> 8) & 0xFF);
    addr_packet[2] = (uint8_t)((ram_addr >> 16) & 0xFF);
    addr_packet[3] = (uint8_t)((ram_addr >> 24) & 0xFF);

    if (serial_write(port, addr_packet, sizeof(addr_packet)) < 0)
        goto error;

    // --- Build payload size packet (little endian)
    uint8_t size_packet[4];
    size_packet[0] = (uint8_t)(fsize & 0xFF);
    size_packet[1] = (uint8_t)((fsize >> 8) & 0xFF);
    size_packet[2] = (uint8_t)((fsize >> 16) & 0xFF);
    size_packet[3] = (uint8_t)((fsize >> 24) & 0xFF);

    if (serial_write(port, size_packet, sizeof(size_packet)) < 0)
        goto error;

    // --- Send loader body
    if (serial_write_chunks(port, buffer, fsize, 0x400) < 0)
        goto error;

    // --- Read hello response from loader =)
    uint8_t hello_buf[128];
    int rcv_len = serial_wait_packet(port, hello_buf, sizeof(hello_buf), 5 * TIMEOUT);
    if (rcv_len <= 0)
        return -1;

    struct packetdata_t repl;
    if (cmd_decode_packet(hello_buf, rcv_len, &repl) != 0)
        return -1;

    loader_get_hello(&repl);

    free(buffer);
    return 0;

error:
    free(buffer);
    return -1;
}

int loader_break_db2000_cid36(struct sp_port *port, struct phone_info *phone)
{
    printf("Breaking rabbit hole...=) \n");

    if (loader_send_binary(port, phone, DB2000_CERTLOADER_RED_CID00_R3L) != 0)
        return -1;

    if (loader_send_binary_cmd3e(port, DB2000_BREAK_R1F) != 0)
        return -1;

    printf("Security disabled =)\n");

    return 0;
}

int loader_break_db2010_cid36(struct sp_port *port, struct phone_info *phone)
{
    printf("Breaking rabbit hole...=) \n");

    if (loader_send_binary(port, phone, DB2010_CERTLOADER_RED_CID01_R2E) != 0)
        return -1;

    if (loader_send_binary_cmd3e(port, DB2010_BREAK_R2E) != 0)
        return -1;

    printf("Security disabled =)\n");

    return 0;
}

// --- Loader activate
int loader_activate_payload(struct sp_port *port, struct phone_info *phone)
{
    if (loader_type == LDR_CHIPSELECT)
    {
        uint8_t cmd_buf[64];
        uint8_t resp[128];
        struct packetdata_t repl = {0};

        int cmd_len, rcv_len;

        // --- Activate CS loader
        printf("Activating CHIPSELECT loader... ");
        cmd_len = cmd_encode_csloader_packet(0x01, 0x09, NULL, 0, cmd_buf);
        if (cmd_len <= 0)
            return -1;
        if (serial_send_packetdata_ack(port, cmd_buf, cmd_len) < 0)
            return -1;

        rcv_len = serial_wait_packet(port, resp, 8, 20 * TIMEOUT);
        if (rcv_len <= 0)
            return -1;

        if (cmd_decode_packet(resp, rcv_len, &repl) != 0)
            return -1;

        if (repl.data[1] != 0x00)
        {
            fprintf(stderr, "failed activating loader\n");
            return -1;
        }
        printf("activated\n");

        // --- Activate GDFS server
        printf("Activating GDFS server... ");
        cmd_len = cmd_encode_csloader_packet(0x04, 0x05, NULL, 0, cmd_buf);
        if (cmd_len <= 0)
            return -1;

        if (serial_send_packetdata_ack(port, cmd_buf, cmd_len) < 0)
            return -1;

        rcv_len = serial_wait_packet(port, resp, 8, 500 * TIMEOUT);
        if (rcv_len <= 0)
            return -1;

        if (cmd_decode_packet(resp, rcv_len, &repl) != 0)
            return -1;

        if (repl.data[1] != 0x00)
        {
            fprintf(stderr, "failed activating GDFS\n");
            return -1;
        }
        printf("activated\n");

        // --- Test unlock command should print phone name if unlocked
        printf("Check loader... ");
        uint8_t gdfsvar[3];
        switch (phone->chip_id)
        {
        case DB2000:
            gdfsvar[0] = phone->is_z1010 ? 0x04 : 0x02;
            gdfsvar[1] = 0x8F;
            gdfsvar[2] = 0x0C;
            break;

        case DB2010_1:
        case DB2010_2:
            gdfsvar[0] = 0x02;
            gdfsvar[1] = 0x8F;
            gdfsvar[2] = 0x0C;
            break;

        case DB2020:
        case PNX5230:
            gdfsvar[0] = 0x02;
            gdfsvar[1] = 0xBB;
            gdfsvar[2] = 0x0D;
            break;
        }

        cmd_len = cmd_encode_csloader_packet(0x04, 0x01, gdfsvar, sizeof(gdfsvar), cmd_buf);
        if (cmd_len <= 0)
            return -1;

        if (serial_send_packetdata_ack(port, cmd_buf, cmd_len) < 0)
            return -1;

        rcv_len = serial_wait_packet(port, resp, sizeof(resp), 10 * TIMEOUT);
        if (rcv_len <= 0)
            return -1;

        if (cmd_decode_packet(resp, rcv_len, &repl) != 0)
            return -1;

        if (repl.cmd != 0x04)
        {
            printf("locked\n");
            return -1;
        }
        else
        {
            wcstombs(phone->phone_name, (wchar_t *)(repl.data + 2), repl.length);
            printf("unlocked:%s\n", phone->phone_name);
        }

        return 0;
    }

    // ------------------- Non-CS Loader -------------------

    // Get Flash data
    if (loader_get_flash_data(port, phone) != 0)
        return -1;

    // Get OTP data
    if (loader_get_otp_data(port, phone) != 0)
        return -1;

    return 0;
}

int loader_send_qhldr_noact(struct sp_port *port, struct phone_info *phone, const char *loader_name)
{
    FILE *fh = fopen(loader_name, "rb");
    if (!fh)
    {
        fprintf(stderr, "Loader file (%s) does not exist!\n", loader_name);
        return -1;
    }

    // printf("Sending %s ... \n", loader_name);

    // Read entire loader into memory
    fseek(fh, 0, SEEK_END);
    uint32_t fsize = ftell(fh);
    fseek(fh, 0, SEEK_SET);

    uint8_t *buffer = malloc(fsize);
    if (!buffer)
    {
        fclose(fh);
        fprintf(stderr, "No memory for loader (%d bytes)\n", fsize);
        return -1;
    }

    fread(buffer, 1, fsize, fh);
    fclose(fh);

    struct babehdr_t *babehdr = (struct babehdr_t *)buffer;

    int qh_size = sizeof(struct babehdr_t);
    int qa_size = babehdr->prologuesize1;
    int qd_size = babehdr->payloadsize1;

    uint8_t *qh00 = buffer;
    uint8_t *qa00 = buffer + qh_size;
    uint8_t *qd00 = buffer + qh_size + qa_size;

    // --- Send QH00
    // printf("Send header ...\n");
    if (serial_write(port, (uint8_t *)"QH00", 4) < 0)
        goto error;
    if (wait_e3_answer(port, "EsB", 3 * TIMEOUT, phone->skiperrors) < 0)
        goto error;
    if (serial_write(port, qh00, qh_size) < 0)
        goto error;
    if (wait_e3_answer(port, "EhM", 3 * TIMEOUT, phone->skiperrors) < 0)
        goto error;

    // --- Send QA00
    // printf("Send prologue ...\n");
    if (serial_write(port, (uint8_t *)"QA00", 4) < 0)
        goto error;
    if (serial_write_chunks(port, qa00, qa_size, 0x800) < 0)
        goto error;
    if (wait_e3_answer(port, "EaT", 3 * TIMEOUT, phone->skiperrors) < 0)
        goto error;
    if (wait_e3_answer(port, "EbS", 3 * TIMEOUT, phone->skiperrors) < 0)
        goto error;

    // --- Send QD00
    // printf("Send body ...\n");
    if (serial_write(port, (uint8_t *)"QD00", 4) < 0)
        goto error;
    if (serial_write_chunks(port, qd00, qd_size, 0x800) < 0)
        goto error;
    if (wait_e3_answer(port, "EdQ", 3 * TIMEOUT, phone->skiperrors) < 0)
        goto error;

    if (phone->skiperrors == 1) // anycid exploit
    {
        phone->anycid = 1;
        uint8_t byteleft;
        int rcv_len = serial_wait_packet(port, &byteleft, 1, 3 * TIMEOUT);
        if (rcv_len <= 0)
            goto error;

        printf("STARTING BOOTLOADER...\n");
        if (serial_write(port, (uint8_t *)"R", 1) < 0)
            goto error;

        if (phone->chip_id == DB2010_2)
        {
            uint8_t twice3E[2];
            int rcv_len = serial_wait_packet(port, twice3E, sizeof(twice3E), 10 * TIMEOUT);
            if (rcv_len <= 0)
                goto error;

            if (serial_write(port, (uint8_t *)"R", 1) < 0)
                goto error;
        }
        else if (phone->chip_id == DB2020)
        {
            uint8_t three3E[3];
            int rcv_len = serial_wait_packet(port, three3E, sizeof(three3E), 10 * TIMEOUT);
            if (rcv_len <= 0)
                goto error;
        }

        uint8_t hello_buf[256];
        rcv_len = serial_wait_packet(port, hello_buf, sizeof(hello_buf), 3 * TIMEOUT);
        if (rcv_len <= 0)
            goto error;

        struct packetdata_t repl;
        if (cmd_decode_packet(hello_buf, rcv_len, &repl) != 0)
            goto error;

        goto succedd;
    }

    // --- Read hello response from loader =)
    uint8_t hello_buf[256];
    int rcv_len = serial_wait_packet(port, hello_buf, sizeof(hello_buf), 3 * TIMEOUT);
    if (rcv_len <= 0)
        goto error;

    struct packetdata_t repl;
    if (hello_buf[0] == SERIAL_HDR89)
    {
        if (cmd_decode_packet_noack(hello_buf, rcv_len, &repl) != 0)
            goto error;
    }
    else if (hello_buf[0] == SERIAL_ACK && hello_buf[1] == SERIAL_HDR89)
    {
        if (cmd_decode_packet_ack(hello_buf, rcv_len, &repl) != 0)
            goto error;
    }
    else
    {
        fprintf(stderr, "[loader_send_qhldr_noact] Unexpected reply %X %X\n", hello_buf[0], hello_buf[1]);
        goto error;
    }

succedd:
    loader_get_hello(&repl);

    free(buffer);
    return 0;

error:
    free(buffer);
    return -1;
}

int loader_send_qhldr(struct sp_port *port, struct phone_info *phone, const char *loader_name)
{
    if (loader_send_qhldr_noact(port, phone, loader_name) != 0)
        return -1;

    if (loader_activate_payload(port, phone) != 0)
        return -1;

    printf("FLASH ID: 0x%x (%s)\n", phone->flash_id, get_flash_vendor(phone->flash_id));

    printf("OTP: LOCKED:%d CID:%d PAF:%d IMEI:%s\n",
           phone->otp_locked,
           phone->otp_cid,
           phone->otp_paf,
           phone->otp_imei);

    if (phone->skip_cmd == 0)
    {
        if (loader_get_erom_data(port, phone) != 0)
            return -1;

        printf("ACTIVE CID:%02d COLOR:%s\n", phone->erom_cid, color_get_name(phone->erom_color));
    }

    return 0;
}

int loader_send_encoded_cmd_and_data(struct sp_port *port, uint8_t cmd, uint8_t *data, int total_bytes)
{
    uint8_t buf[0x800];
    int sent_bytes = 0;
    int do_once = 1;

    // printf("%s ... (%d bytes)\n", desc, total_bytes);

    serial_send_ack(port);

    while (sent_bytes < total_bytes || do_once)
    {
        int num = 0;
        uint8_t checksum = 0;
        do_once = 0;

        buf[num++] = SERIAL_HDR89; // hdr
        buf[num++] = cmd;          // command

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
        if (serial_write(port, buf, num) < 0)
            return -1;

        // --- Read response
        if (continuebit)
        {
            if (serial_wait_ack(port, TIMEOUT))
                return -1;
        }

        sent_bytes += bytes_to_send;
    }

    return 0;
}

int loader_send_binary_noact(struct sp_port *port, const char *loader_name)
{
    FILE *fh = fopen(loader_name, "rb");
    if (!fh)
    {
        fprintf(stderr, "Loader file (%s) does not exist!\n", loader_name);
        return -1;
    }

    // printf("Sending %s ... \n", loader_name);

    // Read entire loader into memory
    fseek(fh, 0, SEEK_END);
    uint32_t fsize = ftell(fh);
    fseek(fh, 0, SEEK_SET);

    uint8_t *buffer = malloc(fsize);
    if (!buffer)
    {
        fclose(fh);
        fprintf(stderr, "No memory for loader (%d bytes)\n", fsize);
        return -1;
    }

    fread(buffer, 1, fsize, fh);
    fclose(fh);

    struct babehdr_t *babehdr = (struct babehdr_t *)buffer;

    int qh_size = sizeof(struct babehdr_t);
    int qa_size = babehdr->prologuesize1;
    int qd_size = babehdr->payloadsize1;

    uint8_t *qh00 = buffer;
    uint8_t *qa00 = buffer + qh_size;
    uint8_t *qd00 = buffer + qh_size + qa_size;

    uint8_t resp[7] = {0};
    int rcv_len;

    struct packetdata_t repl;

    // printf("Send header ...\n");
    if (loader_send_encoded_cmd_and_data(port, 0x3C, qh00, qh_size) < 0)
        goto error;
    rcv_len = serial_wait_packet(port, resp, sizeof(resp), 5 * TIMEOUT);
    if (rcv_len <= 0)
        goto error;

    if (cmd_decode_packet(resp, rcv_len, &repl) != 0)
        return -1;

    if (repl.hdr != SERIAL_HDR89 || repl.cmd != 0x3D || repl.data[0] != 0)
    {
        fprintf(stderr, "Bad answer %02X\n", repl.data[0]);
        return -1;
    }

    // printf("Send prologue ...\n");
    if (loader_send_encoded_cmd_and_data(port, 0x3C, qa00, qa_size) < 0)
        goto error;

    rcv_len = serial_wait_packet(port, resp, sizeof(resp), 5 * TIMEOUT);
    if (rcv_len <= 0)
        goto error;

    if (cmd_decode_packet(resp, rcv_len, &repl) != 0)
        return -1;

    if (repl.hdr != SERIAL_HDR89 || repl.cmd != 0x3D || repl.data[0] != 0)
    {
        fprintf(stderr, "Bad answer %02X\n", repl.data[0]);
        return -1;
    }

    // printf("Send body ...\n");
    if (loader_send_encoded_cmd_and_data(port, 0x3C, qd00, qd_size) < 0)
        goto error;
    rcv_len = serial_wait_packet(port, resp, sizeof(resp), 20 * TIMEOUT);
    if (rcv_len <= 0)
        goto error;

    if (cmd_decode_packet(resp, rcv_len, &repl) != 0)
        return -1;

    if (repl.hdr != SERIAL_HDR89 || repl.cmd != 0x3D || repl.data[0] != 0)
    {
        fprintf(stderr, "Bad answer %02X\n", repl.data[0]);
        return -1;
    }

    // Send ACK to start binary loader
    serial_send_ack(port);

    // --- Read hello response from loader
    uint8_t hello_buf[256];
    rcv_len = serial_wait_packet(port, hello_buf, sizeof(hello_buf), 50 * TIMEOUT);
    if (rcv_len <= 0)
        goto error;

    // struct packetdata_t repl;
    if (hello_buf[0] == SERIAL_HDR89)
    {
        if (cmd_decode_packet_noack(hello_buf, rcv_len, &repl) != 0)
            return -1;
    }
    else if (hello_buf[0] == SERIAL_ACK && hello_buf[1] == SERIAL_HDR89)
    {
        if (cmd_decode_packet_ack(hello_buf, rcv_len, &repl) != 0)
            return -1;
    }
    else if (hello_buf[0] == 0 && hello_buf[1] == SERIAL_HDR89) // CSLOADER
    {
        if (cmd_decode_packet_noack(hello_buf + 1, rcv_len - 1, &repl) != 0)
            return -1;
    }
    else if (hello_buf[0] == 0x23 && hello_buf[1] == SERIAL_HDR89) // CSLOADER V23 CHIPID:0x8000
    {
        if (cmd_decode_packet_noack(hello_buf + 1, rcv_len - 1, &repl) != 0)
            return -1;
    }
    else if (hello_buf[0] == 0 && hello_buf[1] == 0x23 && hello_buf[2] == SERIAL_HDR89) // CSLOADER DB2000?
    {
        if (cmd_decode_packet_noack(hello_buf + 2, rcv_len - 2, &repl) != 0)
            return -1;
    }
    else
    {
        fprintf(stderr, "[loader_send_binary_noact] Unexpected reply %X %X\n", hello_buf[0], hello_buf[1]);
        return -1;
    }

    loader_get_hello(&repl);

    free(buffer);
    return 0;

error:
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

int loader_get_erom_data(struct sp_port *port, struct phone_info *phone)
{
    uint8_t cmd_buf[32];
    int cmd_len = cmd_encode_binary_packet(0x57, NULL, 0, cmd_buf);
    if (cmd_len <= 0)
        return -1;

    // --- send command
    if (serial_send_packetdata_ack(port, cmd_buf, cmd_len) < 0)
        return -1;

    uint8_t resp[32];
    int rcv_len = serial_wait_packet(port, resp, sizeof(resp), 3 * TIMEOUT);
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
    // else if (repl.data[1] & 8)
    //     phone->erom_color = BLACK;
    else
    {
        phone->erom_color = BLACK;
        // return -1;
    }

    phone->erom_cid = repl.data[9];

    return 0;
}

int loader_get_otp_data(struct sp_port *port, struct phone_info *phone)
{
    uint8_t cmd_buf[32];
    int cmd_len = cmd_encode_binary_packet(0x24, NULL, 0, cmd_buf);
    if (cmd_len <= 0)
        return -1;

    // --- send command
    if (serial_send_packetdata_ack(port, cmd_buf, cmd_len) < 0)
        return -1;

    uint8_t resp[64];
    int rcv_len = serial_wait_packet(port, resp, sizeof(resp), 3 * TIMEOUT);
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

    if (strstr(phone->otp_imei, "35345600"))
        phone->is_z1010 = 1;

    return 0;
}

int loader_get_flash_data(struct sp_port *port, struct phone_info *phone)
{
    uint8_t cmd_buf[32];
    int cmd_len = cmd_encode_binary_packet(0x0D, NULL, 0, cmd_buf);
    if (cmd_len <= 0)
        return -1;

    // --- send command
    if (serial_send_packetdata_ack(port, cmd_buf, cmd_len) < 0)
        return -1;

    uint8_t resp[32];
    int rcv_len = serial_wait_packet(port, resp, sizeof(resp), 3 * TIMEOUT);
    if (rcv_len <= 0)
        return -1;

    struct packetdata_t repl;
    if (cmd_decode_packet(resp, rcv_len, &repl) != 0)
        return -1;

    if (repl.cmd != 0x0A && repl.length != 2)
        return -1;

    phone->flash_id = (repl.data[0] << 8) | repl.data[1];

    return 0;
}

int loader_activate_gdfs(struct sp_port *port)
{
    printf("Activating GDFS.. ");

    uint8_t cmd_buf[8];
    int cmd_len = cmd_encode_binary_packet(0x22, NULL, 0, cmd_buf);
    if (cmd_len <= 0)
        return -1;

    // --- send command
    if (serial_send_packetdata_ack(port, cmd_buf, cmd_len) < 0)
        return -1;

    // Wait for reply
    uint8_t resp[8];
    int rcv_len = serial_wait_packet(port, resp, sizeof(resp), 5 * TIMEOUT);
    if (rcv_len <= 0)
        return -1;

    struct packetdata_t repl;
    if (cmd_decode_packet(resp, rcv_len, &repl) != 0)
        return -1;

    if (repl.data[0] != 0x00)
    {
        printf("failed\n");
        return -1;
    }

    printf("activated\n");
    return 0;
}

int loader_read_gdfs_var(struct sp_port *port, struct gdfs_data_t *gdfs, int gd_index,
                         uint8_t block, uint8_t lsb, uint8_t msb)
{
    uint8_t cmd_buf[64];
    int cmd_len = cmd_encode_read_gdfs(block, lsb, msb, cmd_buf);
    if (cmd_len <= 0)
        return -1;

    // --- send command
    if (serial_send_packetdata_ack(port, cmd_buf, cmd_len) < 0)
        return -1;

    // --- wait for response packet
    uint8_t resp[128];
    int rcv_len = serial_wait_packet(port, resp, sizeof(resp), TIMEOUT);
    if (rcv_len <= 0)
        return -1;

    struct packetdata_t repl;
    if (cmd_decode_packet(resp, rcv_len, &repl) != 0)
        return -1;

    switch (gd_index)
    {
    case GD_PHONE_NAME:
        wcstombs(gdfs->phone_name, (wchar_t *)(repl.data + 1), repl.length);
        break;
    case GD_BRAND:
        strncpy(gdfs->brand, (char *)(repl.data + 1), repl.length);
        break;
    case GD_CXC_ARTICLE:
        strncpy(gdfs->cxc_article, (char *)(repl.data + 1), repl.length);
        break;
    case GD_CXC_VERSION:
        strncpy(gdfs->cxc_version, (char *)(repl.data + 1), repl.length);
        break;
    case GD_LANGPACK:
        strncpy(gdfs->langpack, (char *)(repl.data + 1), repl.length);
        break;
    case GD_CDA_ARTICLE:
        strncpy(gdfs->cda_article, (char *)(repl.data + 1), repl.length);
        break;
    case GD_CDA_REVISION:
        strncpy(gdfs->cda_revision, (char *)(repl.data + 1), repl.length);
        break;
    case GD_DEF_ARTICLE:
        strncpy(gdfs->default_article, (char *)(repl.data + 1), repl.length);
        break;
    case GD_DEF_VERSION:
        strncpy(gdfs->default_version, (char *)(repl.data + 1), repl.length);
        break;
    }

    return 0;
}

int loader_shutdown(struct sp_port *port)
{
    uint8_t cmd_buf[8];
    int cmd_len = cmd_encode_binary_packet(0x14, NULL, 0, cmd_buf);
    if (cmd_len <= 0)
        return -1;

    printf("Shutdown phone\n");

    if (serial_send_packetdata_ack(port, cmd_buf, cmd_len) < 0)
        return -1;

    uint8_t resp[2];
    int rcvlen = serial_wait_packet(port, resp, sizeof(resp), 10 * TIMEOUT);
    if (rcvlen <= 0)
        return -1;

    printf("Done");
    return 0;
}

// should activate flash_mode when success
int loader_enter_flashmode(struct sp_port *port, struct phone_info *phone)
{
    switch (phone->chip_id)
    {
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

int loader_send_oflash_ldr_pnx5230(struct sp_port *port, struct phone_info *phone)
{
    phone->skip_cmd = 1;
    switch (phone->erom_cid)
    {
    case 51:
        return loader_send_qhldr(port, phone, PNX5230_FLLOADER_RED_CID51_R2A016);
    case 52:
        return loader_send_qhldr(port, phone, PNX5230_FLLOADER_RED_CID52_R2A019);
    case 53:
        return loader_send_qhldr(port, phone, PNX5230_FLLOADER_RED_CID53_R2A022);
    default:
        fprintf(stderr, "[FLLDR PNX5230] Unknown CID! %d\n", phone->erom_cid);
        return -1;
    }
}

int loader_send_csloader_pnx5230(struct sp_port *port, struct phone_info *phone)
{
    if (loader_send_oflash_ldr_pnx5230(port, phone) != 0)
        return -1;

    switch (phone->erom_cid)
    {
    case 51:
        return loader_send_binary(port, phone, PNX5230_CSLOADER_RED_CID51_R3A015);
    case 52:
        return loader_send_binary(port, phone, PNX5230_CSLOADER_RED_CID52_R3A015);
    case 53:
        return loader_send_binary(port, phone, PNX5230_CSLOADER_RED_CID53_R3A016);
    default:
        fprintf(stderr, "[CSLDR PNX5230] Unknown CID! %d\n", phone->erom_cid);
        return -1;
    }
}

int loader_send_csloader_db2020(struct sp_port *port, struct phone_info *phone)
{
    if (loader_send_qhldr(port, phone, DB2020_PILOADER_RED_CID01_P3M) != 0)
        return -1;

    if (phone->erom_color == BROWN)
    {
        if (loader_send_binary(port, phone, DB2020_LOADER_FOR_SETOOL2) != 0)
            return -1;
        return loader_send_binary(port, phone, DB2020_FSLOADER_P5G_SETOOL);
    }

    switch (phone->erom_cid)
    {
    case 49:
        return loader_send_binary(port, phone, DB2020_CSLOADER_RED_CID49_R3A009);
    case 51:
        return loader_send_binary(port, phone, DB2020_CSLOADER_RED_CID51_R3A009);
    case 52:
        return loader_send_binary(port, phone, DB2020_CSLOADER_RED_CID52_R3A009);
    case 53:
        return loader_send_binary(port, phone, DB2020_CSLOADER_RED_CID53_R3A013);
    default:
        fprintf(stderr, "[CSLDR DB2020] Unknown CID! %d\n", phone->erom_cid);
        return -1;
    }
}

int loader_send_csloader_db2010(struct sp_port *port, struct phone_info *phone)
{
    if (phone->erom_cid == 29)
    {
        phone->skip_cmd = 1;
        if (loader_send_qhldr(port, phone, DB2010_CERTLOADER_RED_CID01_R2E) != 0)
            return -1;
        if (loader_send_binary_cmd3e(port, DB2010_BREAK) != 0)
            return -1;
        if (loader_send_unsigned_ldr(port, DB2010_PRODUCTION_R2AB, 0x4C000000) != 0)
            return -1;
        if (loader_send_binary(port, phone, DB2010_CSLOADER_R2C_DEN_PO) != 0)
            return -1;
        return 0;
    }
    if (phone->erom_cid <= 36) // Both RED and BROWN
    {
        if (loader_send_qhldr(port, phone, DB2010_PILOADER_RED_CID00_R2F) != 0)
            return -1;
        if (loader_break_db2010_cid36(port, phone) != 0)
            return -1;
        if (loader_send_binary(port, phone, DB2010_CSLOADER_R2C_DEN_PO) != 0)
            return -1;
        return 0;
    }

    if (phone->erom_color == BROWN)
    {
        switch (phone->erom_cid)
        {
        case 49:
            if (loader_send_qhldr(port, phone, DB2010_PILOADER_RED_CID00_R2AB) != 0)
                return -1;
            if (loader_send_binary(port, phone, DB2010_CSLOADER_BRN_CID49_V26) != 0)
                return -1;
            return 0;
        case 51:
            if (loader_send_qhldr(port, phone, DB2010_PILOADER_RED_CID00_P4D) != 0)
                return -1;
            phone->skip_cmd = 1;
            if (loader_send_binary(port, phone, DB2010_RESPIN_PRODLOADER_SETOOL2) != 0)
                return -1;
            if (loader_send_binary(port, phone, DB2012_CSLOADER_RED_CID51_R3B009) != 0)
                return -1;
            return 0;
        default:
            fprintf(stderr, "[CSLDR DB2010 BROWN] Unknown CID! %d\n", phone->erom_cid);
            return -1;
        }
    }
    else if (phone->erom_color == RED)
    {
        switch (phone->erom_cid)
        {
        case 49:
            if (loader_send_qhldr(port, phone, DB2010_PILOADER_RED_CID00_P3L) != 0)
                return -1;
            if (loader_send_binary(port, phone, DB2010_CSLOADER_RED_CID49_R3A010) != 0)
                return -1;
            return 0;
        case 50:
            if (loader_send_qhldr(port, phone, DB2010_PILOADER_RED_CID00_P4D) != 0)
                return -1;
            if (loader_send_binary(port, phone, DB2012_CSLOADER_RED_CID50_R3B009) != 0)
                return -1;
            return 0;
        case 51:
            if (loader_send_qhldr(port, phone, DB2010_PILOADER_RED_CID00_P4D) != 0)
                return -1;
            if (loader_send_binary(port, phone, DB2012_CSLOADER_RED_CID51_R3B009) != 0)
                return -1;
            return 0;
        case 52:
            if (loader_send_qhldr(port, phone, DB2010_PILOADER_RED_CID00_P4D) != 0)
                return -1;
            if (loader_send_binary(port, phone, DB2012_CSLOADER_RED_CID52_R3B009) != 0)
                return -1;
            return 0;
        case 53:
            if (loader_send_qhldr(port, phone, DB2010_PILOADER_RED_CID00_P4D) != 0)
                return -1;
            if (loader_send_binary(port, phone, DB2012_CSLOADER_RED_CID53_R3B014) != 0)
                return -1;
            return 0;
        default:
            fprintf(stderr, "CID %d not supported yet\n", phone->erom_cid);
            return -1;
        }
    }
    fprintf(stderr, "Domain %s not supported yet\n", color_get_state(phone->erom_color));
    return -1;
}

int loader_send_csloader_db2000(struct sp_port *port, struct phone_info *phone)
{
    // TODO CID16
    switch (phone->erom_cid)
    {
    case 29:
        phone->skip_cmd = 1;
        if (loader_send_qhldr(port, phone, DB2000_CERTLOADER_RED_CID00_R3L) != 0)
            return -1;
        if (loader_send_binary_cmd3e(port, DB2000_BREAK) != 0)
            return -1;
        if (loader_send_unsigned_ldr(port, phone->is_z1010 ? DB2000_VIOLA_PRODUCTION_R2Z : DB2000_PRODUCTION_R2Z, 0) != 0)
            return -1;
        return loader_send_binary(port, phone, phone->is_z1010 ? DB2000_VIOLA_FILE_SYSTEM_LOADER_R1E : DB2000_SEMC_FILE_SYSTEM_LOADER_R2B);

    case 36:
        phone->skip_cmd = 1;
        if (loader_send_qhldr(port, phone, DB2000_PILOADER_RED_CID00_R1F) != 0)
            return -1;
        if (loader_break_db2000_cid36(port, phone) != 0)
            return -1;
        return loader_send_binary(port, phone, DB2000_CSLOADER_R4B_SETOOL);

    case 37:
        if (loader_send_qhldr(port, phone, DB2000_PILOADER_RED_CID00_R2B) != 0)
            return -1;
        return loader_send_binary(port, phone, DB2000_CSLOADER_RED_CID37_P4L);

    case 49:
        if (loader_send_qhldr(port, phone, DB2000_PILOADER_RED_CID00_R2B) != 0)
            return -1;
        return loader_send_binary(port, phone, DB2000_CSLOADER_RED_CID49_P4L);

    default:
        printf("CID %d not supported\n", phone->erom_cid);
        return -1;
    }
}

int loader_send_csloader(struct sp_port *port, struct phone_info *phone)
{
    switch (phone->chip_id)
    {
    case DB2000:
        return loader_send_csloader_db2000(port, phone);
    case DB2010_1:
    case DB2010_2:
        return loader_send_csloader_db2010(port, phone);
    case DB2020:
        return loader_send_csloader_db2020(port, phone);
    case PNX5230:
        return loader_send_csloader_pnx5230(port, phone);
    default:
        fprintf(stderr, "ChipID %X not supported\n", phone->chip_id);
        return -1;
    }

    return 0;
}

int loader_send_oflash_ldr_db2000(struct sp_port *port, struct phone_info *phone)
{
    // TODO CID16
    // CID29 (both RED and BROWN)
    if (phone->erom_cid == 29)
    {
        phone->skip_cmd = 1;
        if (loader_send_qhldr(port, phone, DB2000_CERTLOADER_RED_CID00_R3L) != 0)
            return -1;
        if (loader_send_binary_cmd3e(port, DB2000_BREAK) != 0)
            return -1;
        if (loader_send_unsigned_ldr(port, phone->is_z1010 ? DB2000_VIOLA_PRODUCTION_R2Z : DB2000_PRODUCTION_R2Z, 0) != 0)
            return -1;
        return 0;
    }
    // CID36 (both RED and BROWN)
    if (phone->erom_cid == 36)
    {
        if (loader_send_qhldr(port, phone, DB2000_PILOADER_RED_CID00_R1F) != 0)
            return -1;
        if (loader_break_db2000_cid36(port, phone) != 0)
            return -1;
        if (loader_send_binary(port, phone, DB2000_FLLOADER_R2B_DEN_PO) != 0)
            return -1;
    }

    if (loader_send_qhldr(port, phone, DB2000_PILOADER_RED_CID00_R2B) != 0)
        return -1;

    switch (phone->erom_cid)
    {
    case 37:
        return loader_send_binary(port, phone, DB2000_FLLOADER_RED_CID37_R2B);
    case 49:
        return loader_send_binary(port, phone, DB2000_FLLOADER_RED_CID49_R2B);
    default:
        printf("[OFLASH] DB2000 CID:%d not supported\n", phone->erom_cid);
        return -1;
    }
}

int loader_send_oflash_ldr_db2010(struct sp_port *port, struct phone_info *phone)
{
    // K500/K700
    if (phone->erom_cid == 29)
    {
        phone->skip_cmd = 1;
        if (loader_send_qhldr(port, phone, DB2010_CERTLOADER_RED_CID01_R2E) != 0)
            return -1;
        if (loader_send_binary_cmd3e(port, DB2010_BREAK) != 0)
            return -1;
        if (loader_send_unsigned_ldr(port, DB2010_PRODUCTION_R2AB, 0x4C000000) != 0)
            return -1;
        return 0;
    }

    // CID36 and lower (both RED and BROWN)
    if (phone->erom_cid <= 36)
    {
        if (loader_send_qhldr(port, phone, DB2010_PILOADER_RED_CID00_R2F) != 0)
            return -1;
        if (loader_break_db2010_cid36(port, phone) != 0)
            return -1;
        if (loader_send_binary(port, phone, DB2010_FLLOADER_P5G_DEN_PO) != 0)
            return -1;

        return 0;
    }

    if (phone->erom_color == BROWN)
    {
        phone->skip_cmd = 1;
        switch (phone->erom_cid)
        {
        // DB2010
        case 49:
            if (loader_send_qhldr(port, phone, DB2010_PILOADER_BROWN_CID49_R1A002) != 0)
                return -1;
            return loader_send_binary(port, phone, DB2020_FLLOADER_R2B_DEN_PO);
        // DB2012
        case 51:
            if (loader_send_qhldr(port, phone, DB2012_PILOADER_BROWN_CID51_R1A002) != 0)
                return -1;
            return loader_send_binary(port, phone, DB2010_FLLOADER_P5G_DEN_PO);

        default:
            fprintf(stderr, "[DB201x BROWN] CID:%d is not supported\n", phone->erom_cid);
            return -1;
        }
    }

    // Fallback RED >= 49
    switch (phone->erom_cid)
    {
    // DB2010
    case 49:
        if (loader_send_qhldr(port, phone, DB2010_PILOADER_RED_CID00_P3L) != 0)
            return -1;
        return loader_send_binary(port, phone, DB2010_FLLOADER_RED_CID49_R2A007);
    // DB2012
    case 50:
        if (loader_send_qhldr(port, phone, DB2010_PILOADER_RED_CID00_P4D) != 0)
            return -1;
        return loader_send_binary(port, phone, DB2012_FLLOADER_RED_CID50_R1A002);
    case 51:
        if (loader_send_qhldr(port, phone, DB2010_PILOADER_RED_CID00_P4D) != 0)
            return -1;
        return loader_send_binary(port, phone, DB2012_FLLOADER_RED_CID51_R2B012);
    case 52:
        if (loader_send_qhldr(port, phone, DB2010_PILOADER_RED_CID00_P4D) != 0)
            return -1;
        return loader_send_binary(port, phone, DB2012_FLLOADER_RED_CID52_R2B012);
    case 53:
        if (loader_send_qhldr(port, phone, DB2010_PILOADER_RED_CID00_P4D) != 0)
            return -1;
        return loader_send_binary(port, phone, DB2012_FLLOADER_RED_CID53_R2B017);
    default:
        fprintf(stderr, "[DB201x RED] CID:%d is not supported\n", phone->erom_cid);
        return -1;
    }
}

int loader_send_oflash_ldr_db2020(struct sp_port *port, struct phone_info *phone)
{
    if (loader_send_qhldr(port, phone, DB2020_PILOADER_RED_CID01_P3M) != 0)
        return -1;

    if (phone->erom_color == BROWN)
    {
        if (loader_send_binary(port, phone, DB2020_PILOADER_BROWN_CID49_SETOOL) != 0)
            return -1;
        return loader_send_binary(port, phone, DB2020_FLLOADER_R2A005_DEN_PO);
    }

    switch (phone->erom_cid)
    {
    case 49:
        return loader_send_binary(port, phone, DB2020_FLLOADER_RED_CID49_R2A005);
    case 51:
        return loader_send_binary(port, phone, DB2020_FLLOADER_RED_CID51_R2A005);
    case 52:
        return loader_send_binary(port, phone, DB2020_FLLOADER_RED_CID52_R2A005);
    case 53:
        return loader_send_binary(port, phone, DB2020_FLLOADER_RED_CID53_R2A015);
    default:
        fprintf(stderr, "[OFLASH] DB2020 CID:%d not supported\n", phone->erom_cid);
        return -1;
    }
}

int loader_send_oflash_ldr(struct sp_port *port, struct phone_info *phone)
{
    switch (phone->chip_id)
    {
    case DB2000:
        return loader_send_oflash_ldr_db2000(port, phone);
    case DB2010_1:
    case DB2010_2:
        return loader_send_oflash_ldr_db2010(port, phone);
    case DB2020:
        return loader_send_oflash_ldr_db2020(port, phone);
    case PNX5230:
        return loader_send_oflash_ldr_pnx5230(port, phone);
    default:
        fprintf(stderr, "Unknown CHIPID %X\n", phone->chip_id);
        return -1;
    }
}

int loader_send_bflash_ldr(struct sp_port *port, struct phone_info *phone)
{
    switch (phone->chip_id)
    {
    case DB2000: // TODO CID16
        if (phone->erom_cid == 29)
        {
            phone->skip_cmd = 1;
            if (loader_send_qhldr(port, phone, DB2000_CERTLOADER_RED_CID00_R3L) != 0)
                return -1;
            if (loader_send_binary_cmd3e(port, DB2000_BREAK) != 0)
                return -1;
            if (loader_send_unsigned_ldr(port, phone->is_z1010 ? DB2000_VIOLA_PRODUCTION_R2Z : DB2000_PRODUCTION_R2Z, 0) != 0)
                return -1;
            return 0;
        }
        else if (phone->erom_cid == 36)
        {
            if (loader_send_qhldr(port, phone, DB2000_PILOADER_RED_CID00_R1F) != 0)
                return -1;
            if (loader_break_db2000_cid36(port, phone) != 0)
                return -1;
            if (loader_send_binary(port, phone, DB2000_FLLOADER_R2B_DEN_PO) != 0)
                return -1;
            return 0;
        }
        else if (phone->erom_cid == 49 && phone->erom_color == BROWN)
        {
            if (loader_send_qhldr(port, phone, DB2000_PILOADER_RED_CID00_R1F) != 0)
                return -1;
            if (loader_break_db2000_cid36(port, phone) != 0)
                return -1;
            if (loader_send_binary(port, phone, DB2000_FLLOADER_R2B_DEN_PO) != 0)
                return -1;
            return 0;
        }
        break;
    case DB2010_1:
    case DB2010_2:
        if (phone->erom_cid == 29) // K500/K700
        {
            phone->skip_cmd = 1;
            if (loader_send_qhldr(port, phone, DB2010_CERTLOADER_RED_CID01_R2E) != 0)
                return -1;
            if (loader_send_binary_cmd3e(port, DB2010_BREAK) != 0)
                return -1;
            if (loader_send_unsigned_ldr(port, DB2010_PRODUCTION_R2AB, 0x4C000000) != 0)
                return -1;
            return 0;
        }
        else if (phone->erom_cid == 36)
        {
            if (loader_send_qhldr(port, phone, DB2010_PILOADER_RED_CID00_R2F) != 0)
                return -1;
            if (loader_break_db2010_cid36(port, phone) != 0)
                return -1;
            if (loader_send_binary(port, phone, DB2010_FLLOADER_P5G_DEN_PO) != 0)
                return -1;
            return 0;
        }
        else if (phone->erom_cid == 49 && phone->erom_color == BROWN)
        {
            phone->skip_cmd = 1;
            if (loader_send_qhldr(port, phone, DB2010_PILOADER_BROWN_CID49_R1A002) != 0)
                return -1;
            if (loader_send_binary(port, phone, DB2020_FLLOADER_R2B_DEN_PO) != 0)
                return -1;
            return 0;
        }
        else if (phone->erom_cid == 51 && phone->erom_color == BROWN)
        {
            phone->skip_cmd = 1;
            if (loader_send_qhldr(port, phone, DB2010_PILOADER_RED_CID00_P4D) != 0)
                return -1;

            if (loader_send_binary(port, phone, DB2010_RESPIN_PRODLOADER_SETOOL2) != 0)
                return -1;
            return 0;
        }
        else if (phone->erom_color == RED &&
                 (phone->erom_cid >= 49 || phone->anycid == 1)) // ANYCID targets RED CID49+
        {
            phone->skiperrors = 1;
            phone->skip_cmd = 1;

            if (loader_send_qhldr(port, phone, DB2010_RESPIN_ID_LOADER_SETOOL2) != 0)
            {
                printf("[QHTRY] Run executer first\n");
                return -1;
            }
            if (loader_send_binary(port, phone, DB2010_RESPIN_PRODLOADER_SETOOL2) != 0)
                return -1;

            printf("Security disabled =)\n");
            return 0;
        }
        break;
    case DB2020:
        if (phone->anycid == 0)
        {
            if (loader_send_qhldr(port, phone, DB2020_PILOADER_RED_CID01_P3M) != 0)
                return -1;
            if (phone->erom_color == BROWN)
            {
                if (loader_send_binary(port, phone, DB2020_PILOADER_BROWN_CID49_SETOOL) != 0)
                    return -1;
                if (loader_send_binary(port, phone, DB2020_FLLOADER_R2A005_DEN_PO) != 0)
                    return -1;
                return 0;
            }
            fprintf(stderr, "This cid & cert is not supported, convert to brown first or break using anycid exploit\n");
            return -1;
        }
        phone->skiperrors = 1;
        phone->skip_cmd = 1;

        if (loader_send_qhldr(port, phone, DB2020_PRELOADER_FOR_SETOOL2) != 0)
        {
            printf("[QHTRY] Run executer first\n");
            return -1;
        }
        if (loader_send_binary(port, phone, DB2020_LOADER_FOR_SETOOL2) != 0)
            return -1;

        printf("Security disabled =)\n");
        return 0;

    case PNX5230:
        phone->skiperrors = 1;
        phone->skip_cmd = 1;

        if (loader_send_qhldr(port, phone, PNX5320_PROLOGUE) != 0)
        {
            printf("[QHTRY] Run executer first\n");
            return -1;
        }
        if (loader_send_binary(port, phone, PNX5230_PRODUCTION) != 0)
            return -1;

        printf("Security disabled =)\n");
        return 0;

    default:
        fprintf(stderr, "Unknown CHIPID %X\n", phone->chip_id);
        return -1;
    }

    fprintf(stderr, "This cid & cert is not supported, convert to brown first\n");
    return -1;
}
