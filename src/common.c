#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h> // _mkdir
#define MKDIR(path) _mkdir(path)
#define RMDIR(path) _rmdir(path)
#define PATHSEP '\\'
#else
#include <unistd.h>
#define MKDIR(path) mkdir(path, 0755)
#define RMDIR(path) rmdir(path)
#define PATHSEP '/'
#endif

#include "babe.h"
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

uint8_t simple_crc_add(const uint8_t *ptr, size_t size)
{
    uint8_t ret = 0;
    for (size_t i = 0; i < size; i++)
    {
        ret += ptr[i];
    }
    return ret;
}

uint8_t simple_crc_xor(const uint8_t *ptr, size_t size)
{
    uint8_t ret = 0;
    for (size_t i = 0; i < size; i++)
    {
        ret ^= ptr[i];
    }
    return ret;
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
    case 0x5C06: // T39/T65
        return "T39";
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

uint32_t get_platform(uint16_t chip_id)
{
    switch (chip_id)
    {
    case DB2000:
        return CHIPID_DB2000;
    case DB2010_1:
    case DB2010_2:
        return CHIPID_DB2010;
    case DB2020:
        return CHIPID_DB2020;
    case PNX5230:
        return CHIPID_PNX5230;
    case DB3150:
        return CHIPID_DB3150;
    default:
        return 0;
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

const char *get_speed_chars(int baudrate)
{
    switch (baudrate)
    {
    case 9600:
        return "S0";
    case 19200:
        return "S1";
    case 38400:
        return "S2";
    case 57600:
        return "S3";
    case 115200:
        return "S4";
    case 230400:
        return "S5";
    case 460800:
        return "S6";
    case 921600:
        return "S7";
    default:
        return NULL;
    }
}

// create backup directory
int create_backup_dir(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0)
    {
        if (S_ISDIR(st.st_mode))
            return 0; // already exists
        fprintf(stderr, "Error: %s exists but is not a directory\n", path);
        return -1;
    }
    if (MKDIR(path) == 0)
    {
        printf("Created backup directory: %s\n", path);
        return 0;
    }
    if (errno == EEXIST) // race condition safety
        return 0;

    perror("mkdir");
    return -1;
}

// check if file is exist
int file_exists(const char *path)
{
    struct stat st;
    return (stat(path, &st) == 0);
}

// check if rest file is exist
int check_restore_file(struct phone_info *phone)
{
    if (strlen(phone->rest_name) == 0)
        return 0;

    char rest_path[256];
    snprintf(rest_path, sizeof(rest_path), "./rest/%s.rest", phone->rest_name);

    return file_exists(rest_path);
}

// check if anycid package is exist
int check_anycid_pkg(struct phone_info *phone)
{
    if (strlen(phone->anycid_target) == 0)
        return 0;

    char anycid_target[256];
    snprintf(anycid_target, sizeof(anycid_target), "./anycid/%s.zip", phone->anycid_target);

    return file_exists(anycid_target);
}

// convert cxc_article string to rest name 
int parse_cxc_article_to_rest_name(struct phone_info *phone, const char *cxc_article)
{
    const char *src = cxc_article;

    // --- Trim leading spaces
    while (*src == ' ' || *src == '\t')
        src++;

    // --- Trim trailing spaces/newlines
    size_t len = strlen(src);
    while (len > 0 && (src[len - 1] == ' ' || src[len - 1] == '\r' || src[len - 1] == '\n'))
        len--;

    char temp[64];
    if (len >= sizeof(temp))
        len = sizeof(temp) - 1;
    memcpy(temp, src, len);
    temp[len] = '\0';

    // --- Find first space separator
    const char *p = temp;
    while (*p && *p != ' ')
        p++;

    // --- Extract version
    size_t version_len = p - temp;
    char version[32] = {0};
    if (version_len > 0 && version_len < sizeof(version))
        memcpy(version, temp, version_len);

    // --- Skip multiple spaces
    while (*p == ' ')
        p++;

    // --- Extract article
    char article[64];
    snprintf(article, sizeof(article), "%.*s", (int)(sizeof(article) - 1), p);

    // --- Build fw_version (safe and warning-free)
    if (version[0] && article[0])
    {
        (void)snprintf(phone->rest_name, sizeof(phone->rest_name),
                       "%.*s_%.*s",
                       (int)(sizeof(phone->rest_name) / 2 - 1), article,
                       (int)(sizeof(phone->rest_name) / 2 - 1), version);
    }
    else
    {
        (void)snprintf(phone->rest_name, sizeof(phone->rest_name),
                       "%.*s", (int)(sizeof(phone->rest_name) - 1), temp);
    }

    return 0;
}

// convert cxc_article string to anycid bypass package 
int parse_cxc_article_to_anycid_pkg(struct phone_info *phone, const char *cxc_article)
{
    // cxc_article contains ASCII like:
    // "R4EA031     prgCXC1250466_GENERIC_DO"
    // "R11AA011    prg1205-7395_CHINA_JH"

    const char *start = strstr(cxc_article, "R"); // All SE firmware version start with 'R'
    const char *prg = strstr(cxc_article, "prg");
    if (!start || !prg)
        return -1;

    // trim spaces before "prg"
    const char *p = prg;
    while (p > start && isspace((unsigned char)*(p - 1)))
        p--;

    char version[16] = {0};
    char article[64] = {0};

    size_t len_ver = p - start;
    if (len_ver >= sizeof(version))
        len_ver = sizeof(version) - 1;
    strncpy(version, start, len_ver);
    version[len_ver] = '\0';

    // skip "prg"
    prg += 3;
    while (*prg && isspace((unsigned char)*prg))
        prg++;

    strncpy(article, prg, sizeof(article) - 1);

    snprintf(phone->anycid_target, sizeof(phone->anycid_target), "%s_%s", version, article);

    return 0;
}

// scan fw buffer for cxc_article  
int scan_fw_version(uint8_t *buf, size_t size, char *fw_id, size_t fw_id_size)
{
    int found = -1;
    for (size_t i = 0; i < size - 3; i++)
    {
        if (memcmp(&buf[i], "prgCXC", 6) == 0 || // DB20XX
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

// helper file loader
uint8_t *load_file(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;

    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    rewind(f);

    uint8_t *buf = malloc(*size);
    if (!buf)
    {
        fclose(f);
        return NULL;
    }

    fread(buf, 1, *size, f);
    fclose(f);
    return buf;
}

// Helper to check if a path is a directory
int is_directory(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode))
        return 1;
    return 0;
}

// convert ascii to ucs2 used by FSX loader
size_t ascii_to_ucs2(const char *src, uint8_t *dst)
{
    size_t i = 0;
    while (src[i] != '\0')
    {
        dst[2 * i] = (uint8_t)src[i];
        dst[2 * i + 1] = 0x00;
        i++;
    }
    // Null terminator (UCS-2: 0x0000)
    dst[2 * i] = 0x00;
    dst[2 * i + 1] = 0x00;
    return (i + 1) * 2; // total bytes written
}

// Extract plain ASCII name from UCS2-LE buffer
int ucs2_to_ascii(const uint8_t *buf, size_t buflen,
                  size_t pos, char *out, size_t outlen,
                  size_t *consumed)
{
    size_t i = 0, o = 0;
    for (;; i++)
    {
        size_t b = pos + i * 2;
        if (b + 1 >= buflen)
            return -1; // truncated

        uint8_t c = buf[b];
        uint8_t hi = buf[b + 1];

        if (c == 0 && hi == 0)
        {
            i++; // include the terminator word
            break;
        }

        if (o + 1 >= outlen)
            return -1;

        out[o++] = (char)c; // just keep low byte
    }

    out[o] = '\0';
    *consumed = i * 2;
    return 0;
}

int ends_with(const char *str, const char *suffix)
{
    if (!str || !suffix)
        return 0;

    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);

    if (lensuffix > lenstr)
        return 0;

    return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

// normalize path string for internal fsx path
void normalize_path(char *path)
{
    char *segments[128]; // stack of path components
    int depth = 0;

    // temporary buffer for writing normalized characters
    char tmp[512];
    char *dst = tmp;

    // Step 1: convert backslashes and collapse duplicates
    const char *src = path;
    char prev = 0;
    while (*src)
    {
        char c = *src++;
        if (c == '\\')
            c = '/';
        if (c == '/' && prev == '/')
            continue;
        *dst++ = c;
        prev = c;
    }
    *dst = '\0';

    // Step 2: split into components and handle "." and ".."
    src = tmp;
    while (*src)
    {
        // skip leading slash
        if (*src == '/')
        {
            src++;
            continue;
        }

        char *seg_start = (char *)src;
        while (*src && *src != '/')
            src++;

        size_t seg_len = src - seg_start;
        if (seg_len == 0)
            continue;

        // Extract segment
        char segment[128];
        memcpy(segment, seg_start, seg_len);
        segment[seg_len] = '\0';

        if (strcmp(segment, ".") == 0)
        {
            // ignore current directory
            continue;
        }
        else if (strcmp(segment, "..") == 0)
        {
            // go up one directory if possible
            if (depth > 0)
                depth--;
        }
        else
        {
            segments[depth++] = strdup(segment); // store pointer
        }
    }

    // Step 3: rebuild final path
    dst = path;
    *dst++ = '/';
    for (int i = 0; i < depth; i++)
    {
        size_t len = strlen(segments[i]);
        memcpy(dst, segments[i], len);
        dst += len;
        if (i < depth - 1)
            *dst++ = '/';
        free(segments[i]);
    }
    *dst = '\0';
}

// clear tmp directory
int remove_recursive(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0)
        return 0;

    if (S_ISDIR(st.st_mode))
    {
        DIR *dir = opendir(path);
        if (!dir)
            return -1;

        struct dirent *entry;
        char fullpath[4096];
        while ((entry = readdir(dir)) != NULL)
        {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

            snprintf(fullpath, sizeof(fullpath), "%s%c%s", path, PATHSEP, entry->d_name);
            remove_recursive(fullpath);
        }
        closedir(dir);
        RMDIR(path);
    }
    else
    {
        remove(path);
    }

    return 0;
}

// Random helpers
static uint32_t rndseed;

void randomize(void)
{
    rndseed = (uint32_t)time(NULL);
}

// Variant 1 — simple LCG with modulo
uint32_t random1(uint32_t arg)
{
    rndseed = rndseed * 0x8088405u + 1;
    return (arg == 0) ? 0 : (rndseed % arg);
}

// Variant 2 — better uniformity (default)
uint32_t random2(uint32_t arg)
{
    rndseed = rndseed * 0x8088405u + 1;
    uint64_t product = (uint64_t)rndseed * arg;
    return (uint32_t)(product >> 32);
}