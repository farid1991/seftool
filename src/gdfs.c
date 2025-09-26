#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <libserialport.h>

#include "common.h"
#include "cmd.h"
#include "gdfs.h"
#include "loader.h"
#include "serial.h"

int gdfs_get_phonename(struct sp_port *port, struct phone_info *phone, struct gdfs_data_t *gdfs)
{
    uint8_t block = 0;
    uint8_t msb = 0;
    uint8_t lsb = 0;

    switch (phone->chip_id)
    {
    case DB2000:
        block = phone->is_z1010 ? 0x04 : 0x02;
        msb = 0x8F;
        lsb = 0x0C;
        break;

    case DB2010_1:
    case DB2010_2:
        block = 0x02;
        msb = 0x8F;
        lsb = 0x0C;
        break;

    case DB2020:
        block = 0x02;
        msb = 0xBB;
        lsb = 0x0D;
        break;

    default:
        return -1;
    }

    return loader_read_gdfs_var(port, gdfs, GD_PHONE_NAME, block, msb, lsb);
}

int gdfs_get_brand(struct sp_port *port, struct phone_info *phone, struct gdfs_data_t *gdfs)
{
    uint8_t block = 0;
    uint8_t msb = 0;
    uint8_t lsb = 0;

    switch (phone->chip_id)
    {
    case DB2000:
        block = phone->is_z1010 ? 0x04 : 0x02;
        msb = 0xB9;
        lsb = 0x0C;
        break;

    case DB2010_1:
    case DB2010_2:
        block = 0x02;
        msb = 0xB9;
        lsb = 0x0C;
        break;

    case DB2020:
        block = 0x02;
        msb = 0xE5;
        lsb = 0x0D;
        break;

    default:
        return -1;
    }

    return loader_read_gdfs_var(port, gdfs, GD_BRAND, block, msb, lsb);
}

int gdfs_parse_simlockdata(struct gdfs_data_t *gdfs, uint8_t *simlock)
{
    gdfs->locked = simlock[0x34] ? 1 : 0;

    // --- Decode MCC and MNC from BCD
    char mccmnc[16];
    decode_bcd(&simlock[0x34], 3, mccmnc, sizeof(mccmnc)); // 3 bytes BCD → 5–6 digits

    // MCC is always first 3 digits
    strncpy(gdfs->mcc, mccmnc, 3);
    gdfs->mcc[3] = '\0';

    // MNC is the rest (2 or 3 digits)
    strncpy(gdfs->mnc, mccmnc + 3, sizeof(gdfs->mnc) - 1);
    gdfs->mnc[sizeof(gdfs->mnc) - 1] = '\0';

    return 0;
}

int gdfs_get_simlock(struct sp_port *port, struct gdfs_data_t *gdfs)
{
    uint8_t block = 0x00;
    uint8_t msb = 0x00;
    uint8_t lsb = 0x06;

    uint8_t cmd_buf[64];
    int cmd_len = cmd_encode_read_gdfs(block, lsb, msb, cmd_buf);
    if (cmd_len < 0)
        return -1;

    if (serial_send_packetdata_ack(port, cmd_buf, cmd_len) < 0)
        return -1;

    uint8_t resp[512];
    int rcv_len = serial_wait_packet(port, resp, sizeof(resp), 5 * TIMEOUT);
    if (rcv_len <= 0)
        return -1;

    struct packetdata_t repl;
    if (cmd_decode_packet(resp, rcv_len, &repl) < 0)
        return -1;

    gdfs_parse_simlockdata(gdfs, repl.data + 1);

    return 0;
}

int gdfs_dump_var(struct sp_port *port, struct phone_info *phone, uint16_t block, uint8_t msb, uint8_t lsb)
{
    uint8_t cmd_buf[64];
    int cmd_len = cmd_encode_read_gdfs(block, lsb, msb, cmd_buf);
    if (cmd_len < 0)
        return -1;

    // --- send command
    if (serial_send_packetdata_ack(port, cmd_buf, cmd_len) < 0)
        return -1;

    // --- wait for response packet
    uint8_t resp[1024];
    int rcv_len = serial_wait_packet(port, resp, sizeof(resp), 5 * TIMEOUT);
    if (rcv_len <= 0)
        return -1;

    struct packetdata_t repl;
    if (cmd_decode_packet(resp, rcv_len, &repl) < 0)
        return -1;

    char backup_path[512];
    snprintf(backup_path, sizeof(backup_path), "./backup/secunits_%s.txt", phone->otp_imei);

    // open backup file in append mode
    FILE *f = fopen(backup_path, "a");
    if (!f)
    {
        fprintf(stderr, "Cannot create backup.txt\n");
        return -1;
    }

    // print header (block + msb + lsb)
    fprintf(f, "gdfswrite:%04X%02X%02X", block, msb, lsb);

    // append hex data
    for (int i = 1; i < repl.length; i++)
    {
        fprintf(f, "%02X", repl.data[i]);
    }

    fprintf(f, "\n");
    fclose(f);

    return 0;
}

int gdfs_dump_sec_units(struct sp_port *port, struct phone_info *phone, const char *backup_name)
{
    if (gdfs_dump_var(port, phone, 0x00, 0x00, 0x06) < 0) // GD_COPS_Dynamic1Variable
        return -1;

    if (gdfs_dump_var(port, phone, 0x00, 0x00, 0x0E) < 0) // GD_COPS_Dynamic2Variable
        return -1;

    if (gdfs_dump_var(port, phone, 0x00, 0x00, 0x13) < 0) // GD_COPS_StaticVariable
        return -1;

    if (gdfs_dump_var(port, phone, 0x00, 0x00, 0x18) < 0) // GD_COPS_ProtectedCustomerSettings
        return -1;

    if (gdfs_dump_var(port, phone, 0x00, 0x00, 0xAA) < 0) // GD_Protected_PlatformSettings
        return -1;

    printf("SECURITY UNITS BACKUP CREATED. %s\n", backup_name);

    return 0;
}

int gdfs_get_cxc_article(struct sp_port *port, struct phone_info *phone, struct gdfs_data_t *gdfs)
{
    uint8_t block = 0;
    uint8_t msb = 0;
    uint8_t lsb = 0;

    switch (phone->chip_id)
    {
    case DB2010_1:
    case DB2010_2:
        block = 0x02;
        msb = 0xE9;
        lsb = 0x0C;
        break;

    case DB2020:
        block = 0x02;
        msb = 0x15;
        lsb = 0x0E;
        break;

    default:
        return -1;
    }

    return loader_read_gdfs_var(port, gdfs, GD_CXC_ARTICLE, block, msb, lsb);
}

int gdfs_get_cxc_version(struct sp_port *port, struct phone_info *phone, struct gdfs_data_t *gdfs)
{
    uint8_t block = 0;
    uint8_t msb = 0;
    uint8_t lsb = 0;

    switch (phone->chip_id)
    {
    case DB2010_1:
    case DB2010_2:
        block = 0x02;
        msb = 0xEA;
        lsb = 0x0C;
        break;

    case DB2020:
        block = 0x02;
        msb = 0x16;
        lsb = 0x0E;
        break;
    default:
        return -1;
    }

    return loader_read_gdfs_var(port, gdfs, GD_CXC_VERSION, block, msb, lsb);
}

int gdfs_get_language(struct sp_port *port, struct phone_info *phone, struct gdfs_data_t *gdfs)
{
    uint8_t block = 0;
    uint8_t msb = 0;
    uint8_t lsb = 0;

    switch (phone->chip_id)
    {
    case DB2000:
        block = phone->is_z1010 ? 0x04 : 0x02;
        msb = 0xBB;
        lsb = 0x0C;
        break;

    case DB2010_1:
    case DB2010_2:
        block = 0x02;
        msb = 0xBB;
        lsb = 0x0C;
        break;

    case DB2020:
        block = 0x02;
        msb = 0xE7;
        lsb = 0x0D;
        break;
    default:
        return -1;
    }

    return loader_read_gdfs_var(port, gdfs, GD_LANGPACK, block, msb, lsb);
}

int gdfs_get_cda_article(struct sp_port *port, struct phone_info *phone, struct gdfs_data_t *gdfs)
{
    uint8_t block = 0;
    uint8_t msb = 0;
    uint8_t lsb = 0;

    switch (phone->chip_id)
    {
    case DB2000:
        block = phone->is_z1010 ? 0x04 : 0x02;
        msb = 0xBC;
        lsb = 0x0C;
        break;

    case DB2010_1:
    case DB2010_2:
        block = 0x02;
        msb = 0xBC;
        lsb = 0x0C;
        break;

    case DB2020:
        block = 0x02;
        msb = 0xE8;
        lsb = 0x0D;
        break;

    default:
        return -1;
    }

    return loader_read_gdfs_var(port, gdfs, GD_CDA_ARTICLE, block, msb, lsb);
}

int gdfs_get_cda_revision(struct sp_port *port, struct phone_info *phone, struct gdfs_data_t *gdfs)
{
    uint8_t block = 0;
    uint8_t msb = 0;
    uint8_t lsb = 0;

    switch (phone->chip_id)
    {
    case DB2000:
        block = phone->is_z1010 ? 0x04 : 0x02;
        msb = 0xBD;
        lsb = 0x0C;
        break;

    case DB2010_1:
    case DB2010_2:
        block = 0x02;
        msb = 0xBD;
        lsb = 0x0C;
        break;

    case DB2020:
        block = 0x02;
        msb = 0xE9;
        lsb = 0x0D;
        break;

    default:
        return -1;
    }

    return loader_read_gdfs_var(port, gdfs, GD_CDA_REVISION, block, msb, lsb);
}

int gdfs_get_default_article(struct sp_port *port, struct phone_info *phone, struct gdfs_data_t *gdfs)
{
    uint8_t block = 0;
    uint8_t msb = 0;
    uint8_t lsb = 0;

    switch (phone->chip_id)
    {
    case DB2000:
        block = phone->is_z1010 ? 0x04 : 0x02;
        msb = 0xBE;
        lsb = 0x0C;
        break;

    case DB2010_1:
    case DB2010_2:
        block = 0x02;
        msb = 0xBE;
        lsb = 0x0C;
        break;

    case DB2020:
        block = 0x02;
        msb = 0xEA;
        lsb = 0x0D;
        break;

    default:
        return -1;
    }

    return loader_read_gdfs_var(port, gdfs, GD_DEF_ARTICLE, block, msb, lsb);
}

int gdfs_get_default_version(struct sp_port *port, struct phone_info *phone, struct gdfs_data_t *gdfs)
{
    uint8_t block = 0;
    uint8_t msb = 0;
    uint8_t lsb = 0;

    switch (phone->chip_id)
    {
    case DB2000:
        block = phone->is_z1010 ? 0x04 : 0x02;
        msb = 0xBF;
        lsb = 0x0C;
        break;

    case DB2010_1:
    case DB2010_2:
        block = 0x02;
        msb = 0xBF;
        lsb = 0x0C;
        break;

    case DB2020:
        block = 0x02;
        msb = 0xEB;
        lsb = 0x0D;
        break;

    default:
        return -1;
    }

    return loader_read_gdfs_var(port, gdfs, GD_DEF_VERSION, block, msb, lsb);
}

int gdfs_get_userlock(struct sp_port *port, struct gdfs_data_t *gdfs)
{
    uint8_t block = 0x00;
    uint8_t msb = 0x00;
    uint8_t lsb = 0x0E;

    uint8_t cmd_buf[64];
    int cmd_len = cmd_encode_read_gdfs(block, lsb, msb, cmd_buf);
    if (cmd_len < 0)
        return -1;

    // --- send command
    if (serial_send_packetdata_ack(port, cmd_buf, cmd_len) < 0)
        return -1;

    // --- wait for response packet
    uint8_t resp[256];
    int rcv_len = serial_wait_packet(port, resp, sizeof(resp), 5 * TIMEOUT);
    if (rcv_len <= 0)
        return -1;

    struct packetdata_t repl;
    if (cmd_decode_packet(resp, rcv_len, &repl) != 0)
        return -1;

    uint8_t usercode_len = repl.data[0x62];
    if (usercode_len == 0)
    {
        strncpy(gdfs->user_lock, "No usercode", sizeof(gdfs->user_lock) - 1);
        gdfs->user_lock[sizeof(gdfs->user_lock) - 1] = '\0';
        return 0;
    }

    char usercode[9] = {0}; // max 8 digits
    int pos = 0;
    for (int i = 0; i < usercode_len; i++)
    {
        int byte_index = 0x63 + (i / 2);
        uint8_t b = repl.data[byte_index];

        uint8_t digit;
        if ((i % 2) == 0)
            digit = b & 0x0F; // low nibble first
        else
            digit = (b >> 4) & 0x0F;

        usercode[pos++] = '0' + digit;
    }
    usercode[pos] = '\0';

    strncpy(gdfs->user_lock, usercode, usercode_len);

    return 0;
}

int gdfs_unlock_usercode(struct sp_port *port)
{
    printf("Reset USERCODE... ");
    uint8_t cmd_buf[64];
    uint8_t resp[64];

    int cmd_len = cmd_encode_csloader_packet(0x01, 0x0D, NULL, 0, cmd_buf);
    if (cmd_len <= 0)
        return -1;

    if (serial_send_packetdata_ack(port, cmd_buf, cmd_len) < 0)
        return -1;

    int rcv_len = serial_wait_packet(port, resp, sizeof(resp), 5 * TIMEOUT);
    if (rcv_len <= 0)
    {
        fprintf(stderr, "failed [no answer]\n");
        return -1;
    }

    struct packetdata_t repl;
    if (cmd_decode_packet(resp, rcv_len, &repl) != 0)
        return -1;

    if (repl.cmd == 0x1 && repl.data[1] == 0)
    {
        printf("done\n\nUSERCODE reset to '0000'\n\n");
        return 0;
    }

    printf("failed\n\n");
    return -1;
}

int gdfs_terminate_access(struct sp_port *port)
{
    uint8_t cmd_buf[8];

    // shutdown
    printf("Terminating GDFS server... ");
    int cmd_len = cmd_encode_csloader_packet(0x01, 0x08, NULL, 0, cmd_buf);
    if (cmd_len <= 0)
        return -1;

    if (serial_send_packetdata_ack(port, cmd_buf, cmd_len) < 0)
        return -1;

    uint8_t resp[8];
    int rcv_len = serial_wait_packet(port, resp, sizeof(resp), 10 * TIMEOUT);
    if (rcv_len <= 0)
    {
        printf("failed\n\n");
        return -1;
    }

    printf("OK\n\n");

    return 0;
}
