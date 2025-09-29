#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "common.h"

// ---------- byte helper ----------
uint8_t get_byte(uint8_t *p)
{
    return p[0];
}

void set_byte(uint8_t *p, uint8_t v)
{
    p[0] = v;
}

uint16_t get_half(uint8_t *p) // 16-bit LE
{
    return (uint16_t)(p[0] | p[1] << 8);
}

void set_half(uint8_t *p, uint16_t v)
{
    p[1] = (v >> 8) & 0xFF;
    p[0] = v & 0xFF;
}

uint32_t get_word(uint8_t *p) // 32-bit LE
{
    return ((uint32_t)p[3] << 24) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[1] << 8) |
           (uint32_t)p[0];
}
void set_word(uint8_t *p, uint32_t v)
{
    p[3] = (v >> 24) & 0xFF;
    p[2] = (v >> 16) & 0xFF;
    p[1] = (v >> 8) & 0xFF;
    p[0] = v & 0xFF;
}

void decode_bcd(const uint8_t *in, int len, char *out, size_t out_size)
{
    size_t pos = 0;
    for (int i = 0; i < len && pos + 1 < out_size; i++)
    {
        uint8_t b = in[i];

        // low nibble
        if ((b & 0x0F) != 0x0F && pos < out_size - 1)
            out[pos++] = '0' + (b & 0x0F);

        // high nibble
        if (((b >> 4) & 0x0F) != 0x0F && pos < out_size - 1)
            out[pos++] = '0' + ((b >> 4) & 0x0F);
    }
    out[pos] = '\0';
}

const char *get_flash_vendor(uint16_t flashid)
{
    uint16_t vendor_id = (flashid >> 8) & 0xFF; // upper byte
    switch (vendor_id)
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

const char *get_chipset_name(uint16_t chip_id)
{
    switch (chip_id)
    {
    case 0x5B07: // "T68/T300/T310/T200/P800";
    case 0x5B08: // "T610/T616/T630/Z600/T2xx/P900"
        return "DB1000";
    case 0x7100:
        return "DB2000";
    case 0x8000:
    case 0x8040:
        return "DB2010";
    case 0x9900:
        return "DB2020";
    case 0xD000:
        return "PNX5230";
    case 0xC802:
        return "DB3150";
    default:
        return "UNKNOWN";
    }
}

const char *color_get_state(int color_code)
{
    switch (color_code)
    {
    case BLUE:
        return "FACTORY";
    case BROWN:
        return "DEVELOPER";
    case RED:
        return "RETAIL";

    default:
        return "BLACK";
    }
}

const char *color_get_name(int color_code)
{
    switch (color_code)
    {
    case BLUE:
        return "BLUE";
    case BROWN:
        return "BROWN";
    case RED:
        return "RED";
    case BLACK:
        return "BLACK";

    default:
        return "BLACK";
    }
}

int scan_fw_version(uint8_t *buf, size_t size, char *fw_id, size_t fw_id_size)
{
    int found = -1;
    for (size_t i = 0; i < size - 3; i++)
    {
        if (memcmp(&buf[i], "prgCXC", 6) == 0 || // DB201X
            memcmp(&buf[i], "prg120", 6) == 0)   // PNX5230
        {
            found = (int)i;
            break;
        }
    }

    if (found == -1)
        return -1;

    size_t j = 0;
    for (; j < fw_id_size - 1 && found + j < size; j++)
    {
        uint8_t c = buf[found + j];
        if (c == 0 || c == '\n' || c == '\r')
            break;
        fw_id[j] = (char)c;
    }
    fw_id[j] = '\0';

    // look for next substring
    int next = found + (int)j;
    while (next < (int)size && buf[next] == 0)
        next++;

    if (next < (int)size && isalnum(buf[next]))
    {
        size_t k = j;
        if (k < fw_id_size - 1)
            fw_id[k++] = '_';

        for (; k < fw_id_size - 1 && next < (int)size; k++, next++)
        {
            uint8_t c = buf[next];
            if (c == 0 || c == '\n' || c == '\r')
                break;
            fw_id[k] = (char)c;
        }
        fw_id[k] = '\0';
    }

    return 0;
}
