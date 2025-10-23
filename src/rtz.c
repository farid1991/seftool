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
#include "rtz_arm.h"
#include "rtz_avr.h"
#include "rtz.h"

/* Serial helper for AVR devices */

// --- 8-bit (Byte) ---
int recv_byte(struct sp_port *port)
{
    uint8_t b = 0;
    int r = serial_wait_packet(port, &b, 1, 20 * TIMEOUT);
    return (r == 1) ? b : -1;
}

void send_byte(struct sp_port *port, uint8_t b)
{
    serial_write(port, &b, 1);
}

// --- 16-bit (Half) ---
void send_half(struct sp_port *port, uint16_t value)
{
    uint8_t buf[2] = {
        (uint8_t)(value >> 8),
        (uint8_t)(value & 0xFF)};
    serial_write(port, buf, 2);
}

int recv_half(struct sp_port *port, uint16_t *value)
{
    uint8_t buf[2];
    int r = serial_wait_packet(port, buf, 2, 20 * TIMEOUT);
    if (r != 2)
        return -1;
    *value = ((uint16_t)buf[0] << 8) | buf[1];
    return 0;
}

// --- 24-bit (mediumword) ---
void send_mediumword(struct sp_port *port, uint32_t value)
{
    uint8_t buf[3] = {
        (uint8_t)((value >> 16) & 0xFF),
        (uint8_t)((value >> 8) & 0xFF),
        (uint8_t)(value & 0xFF)};
    serial_write(port, buf, 3);
}

int recv_mediumword(struct sp_port *port, uint32_t *value)
{
    uint8_t buf[3];
    int r = serial_wait_packet(port, buf, 3, 20 * TIMEOUT);
    if (r != 3)
        return -1;
    *value = ((uint32_t)buf[0] << 16) |
             ((uint32_t)buf[1] << 8) |
             buf[2];
    return 0;
}

// --- 32-bit (word) ---
void send_word(struct sp_port *port, uint32_t value)
{
    uint8_t buf[4] = {
        (uint8_t)(value >> 24),
        (uint8_t)(value >> 16),
        (uint8_t)(value >> 8),
        (uint8_t)(value & 0xFF)};
    serial_write(port, buf, 4);
}

int recv_word(struct sp_port *port, uint32_t *value)
{
    uint8_t buf[4];
    int r = serial_wait_packet(port, buf, 4, 10 * TIMEOUT);
    if (r != 4)
        return -1;
    *value = ((uint32_t)buf[0] << 24) |
             ((uint32_t)buf[1] << 16) |
             ((uint32_t)buf[2] << 8) |
             buf[3];
    return 0;
}

// --- Block (array) ---
void send_block(struct sp_port *port, const void *src, size_t len)
{
    serial_write(port, src, len);
}

void send_chunk(struct sp_port *port, const void *src, size_t len)
{
    serial_write_chunks(port, src, len, 0x100);
}

int recv_block(struct sp_port *port, void *dst, size_t len)
{
    return serial_wait_packet(port, dst, len, 20 * TIMEOUT);
}

int wait_for_byte(struct sp_port *port, uint8_t wb)
{
    const int max_wait = 20 * TIMEOUT;
    int elapsed = 0;
    int b;

    while (elapsed < max_wait)
    {
        b = recv_byte(port);
        if (b == wb)
            return 0; // success
        if (b == -1)
            return -1; // serial error
        elapsed += TIMEOUT;
    }
    return 1; // timeout
}

int wait_for_answer(struct sp_port *port, const char *expected, int skiperrors)
{
    size_t expected_len = strlen(expected);
    uint8_t buf[8];
    size_t received = 0;

    while (received < expected_len)
    {
        int rcv_len = recv_block(port, buf + received, expected_len - received);
        if (rcv_len <= 0)
        {
            if (!skiperrors)
                fprintf(stderr, "[wait_for_answer] recv_block failed (%d)\n", rcv_len);
            return -1;
        }
        received += rcv_len;
    }

    if (memcmp(buf, expected, expected_len) != 0)
    {
        if (!skiperrors)
            fprintf(stderr, "[wait_for_answer] Unexpected reply: got '%.*s', expected '%s'\n",
                    (int)expected_len, buf, expected);
        return -1;
    }

    return 0;
}

static const char *rtz_get_speed_val(int speed)
{
    if (speed >= 921600)
        return "C21";
    if (speed >= 460800)
        return "C23";
    if (speed >= 230400)
        return "C25";
    if (speed >= 115200)
        return "C27";
    return NULL;
}

static int rtz_set_baudrate(struct sp_port *port, int baudrate)
{
    const char *cmd = rtz_get_speed_val(baudrate);
    send_block(port, cmd, 3);

    if (serial_set_baudrate(port, baudrate) != 0)
        return -1;

    sp_flush(port, SP_BUF_BOTH);

    return 0;
}

static int rtz_send_qhldr(struct sp_port *port, const char *fname, int *rsa_active)
{
    char loader_name[256];
    snprintf(loader_name, sizeof(loader_name), "./loader/rtz/%s", fname);

    size_t qhldr_size;
    uint8_t *qhldr = load_file(loader_name, &qhldr_size);

    avr_bin_t *avr = (avr_bin_t *)qhldr;

    size_t qh_size = sizeof(avr_bin_t);
    size_t qa_size = avr->prologue_size0;
    size_t qd_size = avr->payload_size0;

    uint8_t *qh00 = qhldr;
    uint8_t *qa00 = qhldr + qh_size;
    uint8_t *qd00 = qhldr + qh_size + qa_size;

    *rsa_active = 0;

    // --- Send header ...
    send_block(port, "QH00", 4);
    if (wait_for_answer(port, "EsB", 1) != 0)
    {
        // RSA inactive
        *rsa_active = 0;
        free(qhldr);
        return 0;
    }

    // RSA active -> continue
    *rsa_active = 1;

    send_chunk(port, qh00, qh_size);
    if (wait_for_answer(port, "EhM", 1) != 0)
    {
        // RSA inactive
        *rsa_active = 0;
        free(qhldr);
        return 0;
    }

    // RSA active -> continue
    *rsa_active = 1;

    // --- Send prologue
    send_block(port, "QA00", 4);
    send_chunk(port, qa00, qa_size);
    if (wait_for_answer(port, "EaT", 0) != 0)
        goto error;

    // --- Send body
    send_block(port, "QD00", 4);
    send_chunk(port, qd00, qd_size);
    if (wait_for_answer(port, "EdQ", 0) != 0)
        goto error;

    free(qhldr);

    if (wait_for_byte(port, '>') != 0)
        return -1;

    return 0;

error:
    free(qhldr);
    return -1;
}

static int rtz_send_rsabypass(struct sp_port *port)
{
    const char bypass[64] = "./loader/rtz/rsabypass.bin";

    size_t fsize;
    uint8_t *buffer = load_file(bypass, &fsize);
    if (!buffer)
    {
        fprintf(stderr, "can't read %s\n", bypass);
        return -1;
    }

    uint8_t cmd_buf[0x800];
    int cmd_len = cmd_encode_binary_packet(0x23, buffer, fsize, cmd_buf);
    free(buffer);

    if (cmd_len <= 0)
        return -1;

    // --- send cmd23 command
    if (serial_send_packetdata_ack(port, cmd_buf, cmd_len) < 0)
        return -1;

    if (serial_wait_ack(port, 10 * TIMEOUT) != 0)
        return -1;

    if (wait_for_byte(port, 'U') != 0)
        return -1;

    return 0;
}

static void rtz_turn_on_phone(struct sp_port *port)
{
    printf("\nTurning on phone\n\n");
    send_byte(port, STCMD_8_TURNONPHONE);

    printf("Done\n");
}

static int rtz_reconnect(struct sp_port *port, struct phone_info *phone)
{
    struct timespec ts = {0, 500000000};
    phone->avr_ignore_print = 1;
    sp_flush(port, SP_BUF_BOTH);
    serial_close(port);

    printf("\n");
    nanosleep(&ts, NULL);
    nanosleep(&ts, NULL);
    nanosleep(&ts, NULL);
    nanosleep(&ts, NULL);

    /* reopen & handshake */
    if (serial_open(port) != SP_OK)
        return -1;
    if (wait_for_Z(port, phone) != 0)
        return -1;
    if (send_question_mark(port, phone) != 0)
        return -1;

    return 0;
}

#define BIGNUM_SIZE 64
#define MULREZ_SIZE (BIGNUM_SIZE * 2)
#define D41C31D 0x1FF // 511

static int big_cmp(const uint8_t *a, const uint8_t *b, size_t n)
{
    for (size_t i = n; i-- > 0;)
    {
        if (a[i] < b[i])
            return -1;
        if (a[i] > b[i])
            return 1;
    }
    return 0;
}

static void big_sub(uint8_t *out, const uint8_t *a, const uint8_t *b, size_t n)
{
    // out = a - b (assumes a >= b)
    int borrow = 0;
    for (size_t i = 0; i < n; ++i)
    {
        int v = (int)a[i] - (int)b[i] - borrow;
        if (v < 0)
        {
            v += 256;
            borrow = 1;
        }
        else
            borrow = 0;
        out[i] = (uint8_t)v;
    }
}

static void big_add(uint8_t *out, const uint8_t *a, const uint8_t *b, size_t n)
{
    int carry = 0;
    for (size_t i = 0; i < n; ++i)
    {
        int v = (int)a[i] + (int)b[i] + carry;
        out[i] = (uint8_t)(v & 0xFF);
        carry = v >> 8;
    }
}

// add a and b into out, then if out >= mod reduce out -= mod
static void big_add_mod(uint8_t *out, const uint8_t *a, const uint8_t *b, const uint8_t *mod, size_t n)
{
    big_add(out, a, b, n);
    if (big_cmp(out, mod, n) > 0)
    {
        big_sub(out, out, mod, n);
    }
}

// left-shift (multiply by 2) modulo mod: out = (in * 2) mod mod
static void big_double_mod(uint8_t *out, const uint8_t *in, const uint8_t *mod, size_t n)
{
    uint16_t carry = 0;
    for (size_t i = 0; i < n; ++i)
    {
        uint16_t v = ((uint16_t)in[i] << 1) | carry;
        out[i] = (uint8_t)(v & 0xFF);
        carry = (v >> 8) & 0xFF;
    }
    if (big_cmp(out, mod, n) > 0)
    {
        big_sub(out, out, mod, n);
    }
}

// schoolbook multiplication: a * b -> mulrez (2*n bytes)
static void big_mul(uint8_t *mulrez, const uint8_t *a, const uint8_t *b, size_t n)
{
    memset(mulrez, 0, 2 * n);
    for (size_t i = 0; i < n; ++i)
    {
        uint16_t carry = 0;
        for (size_t j = 0; j < n; ++j)
        {
            size_t pos = i + j;
            uint32_t prod = (uint32_t)a[i] * (uint32_t)b[j];
            uint32_t sum = (uint32_t)mulrez[pos] + (prod & 0xFF) + carry;
            mulrez[pos] = (uint8_t)(sum & 0xFF);
            carry = (uint16_t)((prod >> 8) + (sum >> 8));
        }
        // propagate carry
        size_t pos = i + n;
        while (carry)
        {
            uint32_t sum = (uint32_t)mulrez[pos] + carry;
            mulrez[pos] = (uint8_t)(sum & 0xFF);
            carry = (uint16_t)(sum >> 8);
            pos++;
        }
    }
}

const uint8_t somerndarray[BIGNUM_SIZE] = {
    0x6D, 0x4C, 0x4D, 0x5A, 0x73, 0xDF, 0xBB, 0xE0, 0x1A, 0x67, 0x0E, 0xA5, 0x5B, 0xC8, 0xE3, 0x5F,
    0x83, 0x2A, 0x05, 0xB7, 0x6E, 0x0A, 0x22, 0x18, 0x36, 0xCB, 0x41, 0x44, 0x5C, 0x17, 0x73, 0xB2,
    0xB5, 0x74, 0x52, 0xFB, 0x8D, 0xD5, 0xBF, 0xBE, 0x27, 0xB6, 0xA9, 0x96, 0xF0, 0xEB, 0xC1, 0x1E,
    0xFA, 0x50, 0x54, 0x38, 0xD4, 0xE2, 0xC6, 0xBD, 0xD5, 0x5B, 0x51, 0x46, 0x0D, 0xAE, 0x6D, 0x60};

const uint16_t d40B73E[] = {
    0x0003, 0x0005, 0x000B, 0x0013, 0x001F, 0x002B, 0x0035, 0x0043, 0x0047, 0x0061,
    0x0083, 0x0089, 0x009D, 0x00A3, 0x00BF, 0x00DF, 0x00EF, 0x0119, 0x0137, 0x014B};

static int bypassbootauthority_no1st = 0;
static uint8_t mulrez[MULREZ_SIZE];
static uint8_t bignumber1[BIGNUM_SIZE];
static uint8_t bignumber2[BIGNUM_SIZE];
static uint8_t bignumber3[BIGNUM_SIZE];

static void boot4(void)
{
    memset(bignumber1, 0, BIGNUM_SIZE);
    size_t bitpos = (D41C31D - 1);
    size_t byteidx = bitpos >> 3;
    size_t bitoff = bitpos & 7;
    if (byteidx < BIGNUM_SIZE)
    {
        bignumber1[byteidx] = (uint8_t)(1u << bitoff);
    }

    memcpy(bignumber2, mulrez, BIGNUM_SIZE);
    bignumber2[BIGNUM_SIZE - 1] &= 0x7F;

    if (big_cmp(bignumber2, bignumber3, BIGNUM_SIZE) >= 0)
        big_sub(bignumber2, bignumber2, bignumber3, BIGNUM_SIZE);

    ssize_t maxbit = (ssize_t)MULREZ_SIZE * 8;
    int found = 0;
    for (ssize_t byte = MULREZ_SIZE - 1; byte >= 0; --byte)
    {
        uint8_t val = mulrez[byte];
        if (val != 0)
        {
            for (int b = 7; b >= 0; --b)
            {
                if (val & (1u << b))
                {
                    maxbit = (ssize_t)(byte * 8 + b + 1); // number of bits
                    found = 1;
                    break;
                }
            }
        }
        if (found)
            break;
    }

    if (!found)
        maxbit = 0;

    size_t ebx = (D41C31D >> 3);
    uint8_t dl_mask = (uint8_t)(1u << (D41C31D & 7));
    for (ssize_t e = D41C31D; e < maxbit; ++e)
    {
        uint8_t tmp[BIGNUM_SIZE];
        big_double_mod(tmp, bignumber1, bignumber3, BIGNUM_SIZE);
        memcpy(bignumber1, tmp, BIGNUM_SIZE);

        uint8_t al = 0;
        size_t mr_index = ebx;
        if (mr_index < MULREZ_SIZE)
            al = mulrez[mr_index];
        if (al & dl_mask)
        {
            uint8_t tmp2[BIGNUM_SIZE];
            big_add_mod(tmp2, bignumber1, bignumber2, bignumber3, BIGNUM_SIZE);
            memcpy(bignumber2, tmp2, BIGNUM_SIZE);
        }

        dl_mask <<= 1;
        if (dl_mask == 0)
        {
            ebx++;
            dl_mask = 1;
        }
    }
}

void boot2(uint8_t *addr1, uint8_t *addr2, uint32_t currentrnd)
{
    uint8_t var_44[BIGNUM_SIZE];
    randomize();

    memcpy(bignumber3, somerndarray, BIGNUM_SIZE);
    for (size_t i = 0; i < BIGNUM_SIZE; ++i)
        bignumber2[i] = random1(0xFF);
    bignumber2[BIGNUM_SIZE - 1] &= 0x3F;

    memcpy(bignumber1, bignumber2, BIGNUM_SIZE);
    memcpy(var_44, bignumber2, BIGNUM_SIZE);

    big_mul(mulrez, bignumber1, bignumber2, BIGNUM_SIZE);

    memcpy(bignumber2, mulrez, BIGNUM_SIZE);
    bignumber2[BIGNUM_SIZE - 1] &= 0x7F;

    // call boot4
    boot4();

    for (int loop2count = 0; loop2count != 20; loop2count++)
    {
        int lowbit = (currentrnd & 1);
        currentrnd >>= 1;
        if (lowbit)
        {
            memset(bignumber1, 0, BIGNUM_SIZE);

            uint16_t val = d40B73E[loop2count];
            bignumber1[0] = (uint8_t)(val & 0xFF);
            bignumber1[1] = (uint8_t)((val >> 8) & 0xFF);
            big_mul(mulrez, bignumber1, bignumber2, BIGNUM_SIZE);

            boot4();
        }
    }

    memcpy(addr2, bignumber2, BIGNUM_SIZE);
    memcpy(addr1, var_44, BIGNUM_SIZE);
}

int rtz_bypass_boot_authority(struct sp_port *port, struct phone_info *phone)
{
    uint8_t bignum_42_0_1[BIGNUM_SIZE];
    uint8_t bignum_43_0_1[BIGNUM_SIZE];
    uint8_t bignum_43_0_1_tempstorage[BIGNUM_SIZE];
    int randomz[256];
    int randomznum = 0;
    int current_rnd = 0;
    int repeated_rnd = 0;
    uint8_t resp[8];

    int bootstrap_2d_step_done = 0;
    int bootstrap_3d_step_done = 0;
    uint32_t eqcount;
    int rcv_len;
    uint8_t a = 0, b = 0, c = 0;

    memset(bignum_42_0_1, 0, BIGNUM_SIZE);
    memset(bignum_43_0_1, 0, BIGNUM_SIZE);

    serial_set_rts(port);

    printf("Bootstrap First Step\n");
    for (int i = 0; i < 0x28; i++)
    {
        struct timespec ts = {0, 50000000}; // 50 ms sleep
        nanosleep(&ts, NULL);

        printf("Try %d times\n", i + 1);

        if (bypassbootauthority_no1st != 0)
        {
            /* reopen & handshake */
            if (rtz_reconnect(port, phone) != 0)
                return -1;
        }
        else
            bypassbootauthority_no1st++;

        // send 0x50, 0, 0xAB
        a = 0x50, b = 0x00, c = 0xAB;
        uint8_t cmd50[3] = {a, b, c};
        send_block(port, cmd50, sizeof(cmd50));
        rcv_len = recv_block(port, resp, 3);
        if (rcv_len < 3)
        {
            fprintf(stderr, "Wrong reply\n");
            return -1;
        }

        //  send 0x42, 0, 1
        a = 0x42, b = 0x00, c = 0x01;
        uint8_t cmd42[3] = {a, b, c};
        send_block(port, cmd42, sizeof(cmd42));
        send_block(port, bignum_42_0_1, BIGNUM_SIZE);
        uint8_t crc1 = simple_crc_add(bignum_42_0_1, BIGNUM_SIZE) + a + b + c;
        send_byte(port, crc1);
        rcv_len = recv_block(port, resp, 7);
        if (rcv_len < 7)
        {
            fprintf(stderr, "Wrong reply\n");
            return -1;
        }
        current_rnd = (resp[3] << 16) | (resp[4] << 8) | resp[5];

        if (!bootstrap_2d_step_done)
        {
            eqcount = 0;
            for (int j = 0; j < randomznum; ++j)
            {
                if (randomz[j] == current_rnd)
                    eqcount++;
            }
            if (eqcount > 1)
            {
                // do second step
                printf("Bootstrap Second Step ...\n");
                i = 0;
                boot2(bignum_43_0_1_tempstorage, bignum_42_0_1, current_rnd);
                repeated_rnd = current_rnd;
                bootstrap_2d_step_done++;
                printf("Bootstrap Third Step ...\n");
            }
            randomz[randomznum] = current_rnd;
            randomznum++;
        }
        else if (repeated_rnd == current_rnd)
        {
            memcpy(bignum_43_0_1, bignum_43_0_1_tempstorage, BIGNUM_SIZE);
            bootstrap_3d_step_done = 1;
            printf("Bootstrap Fourth Step ...\n");
        }

        // send 0x43, 0, 1
        a = 0x43, b = 0x00, c = 0x01;
        uint8_t cmd43[3] = {a, b, c};
        send_block(port, cmd43, sizeof(cmd43));
        send_block(port, bignum_43_0_1, BIGNUM_SIZE);
        uint8_t crc2 = simple_crc_add(bignum_43_0_1, BIGNUM_SIZE) + a + b + c;
        send_byte(port, crc2);
        rcv_len = recv_block(port, resp, 3);
        if (rcv_len <= 0)
        {
            fprintf(stderr, "Empty reply\n");
            return -1;
        }

        if (!bootstrap_3d_step_done)
        {
            nanosleep(&ts, NULL);
        }
        else
        {
            if (resp[2] != 0x92)
            {
                printf("FAILED\n");
                return -1;
            }
            // We are already bypassed rsa here =)
            printf("SUCCESS\n");
            return 0;
        }
    }
    printf("FAILED\n");
    return -1;
}

int rtz_send_next_loader(struct sp_port *port, const char *fname)
{
    send_block(port, "0B", 2);

    // --- Read response
    if (wait_for_byte(port, 'R') != 0)
        return -1;

    char loader_name[256];
    snprintf(loader_name, sizeof(loader_name), "./loader/rtz/%s", fname);

    size_t ldr_size;
    uint8_t *ldr = load_file(loader_name, &ldr_size);

    send_chunk(port, ldr, ldr_size);
    free(ldr);

    // --- Read response 0x0D 0x0A 0x3E
    if (wait_for_byte(port, '>') != 0)
        return -1;

    return 0;
}

int rtz_boot_up(struct sp_port *port, struct phone_info *phone)
{
    printf("Check if RSA is active\n");
    int rsa_active;
    if (rtz_send_qhldr(port, "CXC1327525_PROD_LOADER_R1A", &rsa_active) != 0)
        return -1;

    if (rsa_active)
    {
        printf("RSA Active, bypass RSA\n");

        if (rtz_set_baudrate(port, phone->baudrate) != 0)
            return -1;
        if (rtz_avr_get_otp_imei(port, phone) != 0)
            return -1;
        if (rtz_send_next_loader(port, "CXC1325345_RECOVERY_LOADER_R3A") != 0)
            return -1;
        if (rtz_avr_activate_loader(port, phone) != 0)
            return -1;
        if (rtz_send_rsabypass(port) != 0)
            return -1;

        goto succedd;
    }

    printf("Bypass boot authority\n");
    if (rtz_bypass_boot_authority(port, phone) != 0)
        return -1;
    if (connection_set_speed(port, phone) != 0)
        return -1;

succedd:
    send_byte(port, 'X');

    if (rtz_avr_send_bootloader(port) != 0)
        return -1;
    if (rtz_avr_set_com_speed(port, phone->baudrate) != 0)
        return -1;

    return 0;
}

int rtz_identify(struct sp_port *port, struct phone_info *phone)
{
    if (rtz_boot_up(port, phone) != 0)
        return -1;

    if (rtz_avr_get_flash_info(port) != 0)
        return -1;

    // Turn-on phone after success every operation
    rtz_turn_on_phone(port);

    return 0;
}

int rtz_flash_arm(struct sp_port *port, struct phone_info *phone, const char *fw_name)
{
    if (rtz_boot_up(port, phone) != 0)
        return -1;

    if (rtz_arm_init_boot(port) != 0)
        return -1;
    if (rtz_arm_send_bootloader(port) != 0)
        return -1;
    if (rtz_arm_set_com_speed(port, phone->baudrate) != 0)
        return -1;
    if (rtz_arm_get_flash_id(port) != 0)
        return -1;

    if (ends_with(fw_name, ".arm"))
    {
        if (rtz_arm_flash(port, fw_name) != 0)
            return -1;
    }
    else
    {
        printf("Unknown firmware type =)\n");
    }

    // Turn-on phone after success every operation
    rtz_turn_on_phone(port);

    return 0;
}

int rtz_flash_avr(struct sp_port *port, struct phone_info *phone, const char *fw_name)
{
    if (rtz_boot_up(port, phone) != 0)
        return -1;

    if (rtz_avr_get_flash_info(port) != 0)
        return -1;

    if (ends_with(fw_name, ".bin"))
    {
        if (rtz_avr_flash_bin(port, 0, fw_name) != 0)
            return -1;
    }
    else if (ends_with(fw_name, ".sbn"))
    {
        printf("Flash SBN is not supported yet, convert to bin first with sbntool\n");
    }
    else
    {
        printf("Unknown firmware type =)\n");
    }

    // Turn-on phone after success every operation
    rtz_turn_on_phone(port);

    return 0;
}

int rtz_rip_flash(struct sp_port *port, const char *fname, int what, uint32_t addr, uint32_t size)
{
    char dump_name[512];
    snprintf(dump_name, sizeof(dump_name), "./backup/%s", fname);

    FILE *out = fopen(dump_name, "wb");
    if (!out)
    {
        fprintf(stderr, "Error: cannot create output file %s\n", dump_name);
        return -1;
    }

    uint8_t buf[4096];
    uint32_t read_size;
    while (size > 0)
    {
        read_size = (size > sizeof(buf)) ? sizeof(buf) : size;
        printf("\rReading block 0x%08X (%u bytes)", addr, read_size);

        switch (what)
        {
        case RIP_ARM:
            send_byte(port, STCMD_15_RIPARMFLASH);
            send_word(port, addr);
            send_word(port, read_size);
            break;

        case RIP_AVR:
            send_byte(port, STCMD_3_RIPAVRFLASH);
            send_mediumword(port, addr); // 24-bit address
            send_half(port, read_size);  // 16-bit size
            break;

        case RIP_AVRPROGRAM:
            send_byte(port, STCMD_C_RIPAVRPROGRAMMEM);
            send_mediumword(port, addr);
            send_half(port, read_size);
            break;

        default:
            fprintf(stderr, "Invalid RIP type %d\n", what);
            fclose(out);
            return -1;
        }

        // Receive data
        if (recv_block(port, buf, read_size) <= 0)
        {
            fprintf(stderr, "\nError: failed to read block data\n");
            fclose(out);
            return -1;
        }

        // Read CRC byte
        int crc = recv_byte(port);
        if (crc < 0)
        {
            fprintf(stderr, "\nError: failed to read CRC\n");
            fclose(out);
            return -1;
        }
        // Expect trailing '>'
        if (wait_for_byte(port, '>') != 0)
        {
            fprintf(stderr, "\nError: missing '>' terminator\n");
            fclose(out);
            return -1;
        }

        // Validate CRC
        if (what == RIP_ARM)
            crc ^= '+';
        else
        {
            for (int i = 0; i < (int)read_size; i++)
                crc ^= buf[i];
        }

        if (crc)
        {
            fprintf(stderr, "\nError: CRC mismatch\n");
            fclose(out);
            return -1;
        }

        addr += read_size;
        size -= read_size;

        // Write to output
        fwrite(buf, 1, read_size, out);
    }

    fclose(out);
    printf("\nDump complete -> %s\n", dump_name);
    return 0;
}
