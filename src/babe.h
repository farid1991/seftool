#ifndef babe_h
#define babe_h

#include <stdint.h>

#pragma pack(push, 1)
struct babehdr_t
{
    uint16_t sig;           // 0xBEBA
    uint8_t unk;            //
    uint8_t ver;            // 03/04
    uint32_t color;         // 00 - red, 0x60 - brown
    uint32_t platform;      //
    uint32_t z1;            // used by bootrom
    uint32_t cid;           //
    uint32_t clr;           // ???????? = 0xC1
    uint32_t f0[9];         // ffffffff
    uint8_t certplace[488]; //
    uint32_t prologuestart; //
    uint32_t prologuesize1; //
    uint32_t prologuesize2; //
    uint32_t unk1[4];       // 0,1,1,0xFFFFFFFF
    uint8_t hash1[128];     //
    uint32_t flags;         // 0x200 - main/fs/sfa/cert [2C0]
    uint32_t unk2[4];       //
    uint32_t clr2;          // ???????? = 0xC1
    uint32_t f1[3];         // ffffffff
    uint32_t payloadstart;  //
    uint32_t payloadsize1;  // numblocks
    uint32_t payloadsize2;  //
    uint32_t flags2;        // 10 - sfa, 1 - main/fs/cert
    uint32_t unk4[3];       // 1,1,0xFFFFFFFF
    uint8_t hash2[128];     //
};
#pragma pack(pop)

enum
{
    CHIPID_DB3150 = 0x00000200,
    CHIPID_DB2000 = 0x00010000,  // 0x0001
    CHIPID_DB2001 = 0x00030000,  // 0x0003
    CHIPID_DB2010 = 0x00100000,  // 0x0010
    CHIPID_DB2012 = 0x00300000,  // 0x0030
    CHIPID_PNX5230 = 0x01000000, // 0x0100
    CHIPID_DB2020 = 0x10000000   // 0x1000
};

enum
{
    COLOR_RED = 0,
    COLOR_BROWN = 0x60,
    COLOR_BLUE = 0xFFFFFFEF,
    COLOR_BLACK = 0x4B434C42,   //'BLCK',  //?????????
    COLOR_UNKNOWN = 0x4E4B4E55, //'UNKN' //?????????
};

int babe_check(uint8_t *file, size_t size, int checktype);
int babe_is_valid(uint8_t *addr, size_t size);

typedef enum
{
    CHECKBABE_CHECKFULL,
    CHECKBABE_CHECKFAST
} babe_checktype_t;

typedef enum
{
    CHECKBABE_NOTBABE,
    CHECKBABE_BADFILE,
    CHECKBABE_CANTCHECK,
    CHECKBABE_NOTFULL,
    CHECKBABE_OK
} babe_result_t;

#endif // babe_h