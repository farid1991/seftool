#ifndef rtz_h
#define rtz_h

enum
{
    STCMD_1_ARMINIT = 0x1,
    STCMD_2_BOOTARM = 0x2,
    STCMD_3_RIPAVRFLASH = 0x3,
    STCMD_04_FLASHAVR_TYPE1 = 0x4,
    STCMD_6_SETRAMBYTE = 0x6,
    STCMD_8_TURNONPHONE = 0x8,
    STCMD_A_ERASEAVR_FLASHTYPE1_STEP1 = 0xA,
    STCMD_B_ERASEAVR_FLASHTYPE1_STEP2 = 0xB,
    STCMD_C_RIPAVRPROGRAMMEM = 0xC,
    STCMD_E_ERASEAVR_FLASHTYPE2_STEP1 = 0xE,
    STCMD_F_ERASEAVR_FLASHTYPE2_STEP2 = 0xF,
    STCMD_10_FLASHAVR_TYPE2 = 0x10,
    STCMD_14_ARMCMDWITHREPLY = 0x14,
    STCMD_15_RIPARMFLASH = 0x15,
    STCMD_16_SETARMCOMSPEED = 0x16,
    STCMD_17_FLASH_ARM_BLOCK = 0x17,
    STCMD_18_SETAVRCOMSPEED = 0x18,
    STCMD_19_GETCFIINFO = 0x19
};

enum
{
    CMD_UNLOCK_DATA_1 = 0x00AA,
    CMD_UNLOCK_DATA_2 = 0x0055,
    CMD_MANUFACTURER_UNLOCK_DATA = 0x0090,
    CMD_UNLOCK_BYPASS_MODE = 0x0020,
    CMD_PROGRAM_UNLOCK_DATA = 0x00A0,
    CMD_RESET_DATA = 0x00F0,
    CMD_SECTOR_ERASE_UNLOCK_DATA = 0x0080,
    CMD_SECTOR_ERASE_UNLOCK_DATA_2 = 0x0030,
    CMD_UNLOCK_SECTOR = 0x0060
};

enum
{
    NAND_CMD_READ0 = 0,
    NAND_CMD_READ1 = 1,
    NAND_CMD_PAGEPROG = 0x10,
    NAND_CMD_READOOB = 0x50,
    NAND_CMD_ERASE1 = 0x60,
    NAND_CMD_STATUS = 0x70,
    NAND_CMD_SEQIN = 0x80,
    NAND_CMD_READID = 0x90,
    NAND_CMD_ERASE2 = 0xd0,
    NAND_CMD_RESET = 0xff
};

enum
{
    RIP_ARM,
    RIP_AVR,
    RIP_AVRPROGRAM
};

// --- 8-bit (Byte) ---
int recv_byte(struct sp_port *port);
void send_byte(struct sp_port *port, uint8_t b);

// --- 16-bit (Half) ---
void send_half(struct sp_port *port, uint16_t value);
int recv_half(struct sp_port *port, uint16_t *value);

// --- 24-bit (mediumword) ---
void send_mediumword(struct sp_port *port, uint32_t value);
int recv_mediumword(struct sp_port *port, uint32_t *value);

// --- 32-bit (word) ---
void send_word(struct sp_port *port, uint32_t value);
int recv_word(struct sp_port *port, uint32_t *value);

// --- Block (array) ---
void send_block(struct sp_port *port, const void *src, size_t len);
void send_chunk(struct sp_port *port, const void *src, size_t len);
int recv_block(struct sp_port *port, void *dst, size_t len);

int wait_for_byte(struct sp_port *port, uint8_t wb);
int wait_for_answer(struct sp_port *port, const char *expected, int skiperrors);

int rtz_identify(struct sp_port *port, struct phone_info *phone);
int rtz_flash_arm(struct sp_port *port, struct phone_info *phone, const char* fw_name);
int rtz_flash_avr(struct sp_port *port, struct phone_info *phone, const char* fw_name);
int rtz_rip_flash(struct sp_port *port, const char *fname, int what, uint32_t addr, uint32_t size);

#endif // rtz_h