#ifndef common_h
#define common_h

#include <stdint.h>

#define TIMEOUT 100 // ms

#define SERIAL_ACK 0x06
#define SERIAL_NAK 0x15
#define SERIAL_HDR89 0x89
#define SERIAL_CMD3C 0x3C
#define SERIAL_CMD3E 0x3E

#define DB2000 0x7100
#define DB2010_1 0x8000
#define DB2010_2 0x8040
#define DB2020 0x9900
#define PNX5230 0xD000

#define BLOCK_SIZE 0x10000

extern int loader_type;
extern int skip_cmd;
extern int skiperrors;
extern int is_z1010;

struct phone_info
{
    // base
    uint16_t chip_id;
    uint8_t protocol_major;
    uint8_t protocol_minor;
    uint8_t new_security;
    char phone_name[8];

    // EROM
    int erom_color;
    int erom_cid;

    // NAND
    int flash_id;

    // OTP
    uint8_t otp_status;
    uint8_t otp_locked;
    uint16_t otp_cid;
    uint8_t otp_paf;
    char otp_imei[15];
};

enum color_e
{
    BLUE,
    BROWN,
    RED,
    BLACK
};

// ---------- byte helper ----------
uint8_t get_byte(uint8_t *p);
void set_byte(uint8_t *p, uint8_t v);
uint16_t get_half(uint8_t *p); // 16-bit LE
void set_half(uint8_t *p, uint16_t v);
uint32_t get_word(uint8_t *p); // 32-bit LE
void set_word(uint8_t *p, uint32_t v);

int isbabe(uint8_t *addr, uint32_t size);

void decode_bcd(const uint8_t *in, int len, char *out, size_t out_size);

const char* get_flash_vendor(uint16_t flashid);
const char *get_chipset_name(uint16_t chip_id);
const char *color_get_state(int color_code);
const char *color_get_name(int color_code);

#endif // common_h