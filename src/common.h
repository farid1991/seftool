#ifndef common_h
#define common_h

#include <stdint.h>

#ifdef _WIN32
#include <direct.h> // _mkdir
#define MKDIR(path) _mkdir(path)
#define PATHSEP '\\'
#else
#include <unistd.h>
#define MKDIR(path) mkdir(path, 0755)
#define RMDIR(path) rmdir(path)
#define PATHSEP '/'
#endif

#define TIMEOUT 100 // ms

#define SERIAL_ACK 0x06
#define SERIAL_NAK 0x15
#define SERIAL_HDR89 0x89
#define SERIAL_CMD3C 0x3C
#define SERIAL_CMD3E 0x3E

#define EMP_PROTOCOL_2 2
#define EMP_PROTOCOL_3 3

#define DB2000 0x7100
#define DB2010_1 0x8000
#define DB2010_2 0x8040
#define DB2020 0x9900
#define PNX5230 0xD000
#define DB3150 0xC802

enum connect_mode_e
{
    SERIAL_CONNECTION,
    USB_CONNECTION
};

struct phone_info
{
    // base
    uint16_t chip_id;
    uint8_t protocol_major;
    uint8_t protocol_minor;
    uint8_t new_security;
    char phone_name[8];
    char cxc_article[64];
    char cxc_version[64];
    char rest_name[64];
    char anycid_target[64];
    int is_z1010;
    int baudrate;
    int connect_mode;

    // EROM
    int erom_color;
    int erom_cid;

    // NAND
    int flash_id;
    size_t flashblocksize;

    // OTP
    uint8_t otp_status;
    uint8_t otp_locked;
    uint16_t otp_cid;
    uint8_t otp_paf;
    char otp_imei[16];

    // state
    int qhldr_sent;
    int save_as_babe;
    int anycid;
    int gdfs_server;

    // break state
    int break_cid29;
    int break_cid36;
    int break_rsa;
    char bootname[32];
    char osename[32];
    char hdrname[32];

    //AVR
    int avr_ignore_print;
    char gdfs_imei[16];
};

// ---------- byte helper ----------
uint8_t get_byte(uint8_t *p);
void set_byte(uint8_t *p, uint8_t v);
uint16_t get_half(uint8_t *p); // 16-bit LE
void set_half(uint8_t *p, uint16_t v);
uint32_t get_word(uint8_t *p); // 32-bit LE
void set_word(uint8_t *p, uint32_t v);

uint8_t simple_crc_add(const uint8_t *ptr, size_t size);
uint8_t simple_crc_xor(const uint8_t *ptr, size_t size);

void decode_bcd(const uint8_t *in, int len, char *out, size_t out_size);

const char *get_speed_chars(int baudrate);
const char *get_flash_vendor(uint16_t flashid);
const char *get_chipset_name(uint16_t chip_id);
const char *color_get_state(int color_code);
const char *color_get_name(int color_code);
uint32_t get_platform(uint16_t chip_id);

int create_backup_dir(const char *path);
int file_exists(const char *path);
int check_anycid_pkg(struct phone_info *phone);
int check_restore_file(struct phone_info *phone);
int parse_cxc_article_to_rest_name(struct phone_info *phone, const char *cxc_article);
int parse_cxc_article_to_anycid_pkg(struct phone_info *phone, const char *cxc_article);
int scan_fw_version(uint8_t *buf, size_t size, char *fw_id, size_t fw_id_size);

uint8_t *load_file(const char *path, size_t *size);
int is_directory(const char *path);

size_t ascii_to_ucs2(const char *src, uint8_t *dst);
int ucs2_to_ascii(const uint8_t *buf, size_t buflen,
                    size_t pos, char *out, size_t outlen,
                    size_t *consumed);

int ends_with(const char *str, const char *suffix);
void normalize_path(char *path);
int remove_recursive(const char *path);

void randomize(void);
uint32_t random1(uint32_t arg);
uint32_t random2(uint32_t arg);

#endif // common_h