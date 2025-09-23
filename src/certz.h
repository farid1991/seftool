#ifndef certz_h
#define certz_h

#include <stdint.h>

#pragma pack(push, 1)
struct A1CERT
{
    uint32_t keysize;      // 1
    char name[0x40];       //
    uint8_t cert0[0x80];   //
    uint8_t cert80[0x80];  //
    uint32_t e;            // 5
    uint32_t serviceflag;  // 0000=blue , 01=red/brown
    uint32_t C1;           // C1 00 00 00
    uint32_t color;        // color: FFFF=blue , 62=brown , 04=red
    uint32_t FF;           // FF 00 00 00
    uint32_t cid;          //
    uint32_t unk0;         //
    uint16_t year;         //
    uint8_t month;         //
    uint8_t day;           //
    uint32_t FFFFFFFF;     // FF FF FF FF
    uint8_t cert124[0x80]; //
};
#pragma pack(pop)

struct certz_t
{
    uint32_t platform;
    uint8_t cid;
    uint8_t color;
    uint8_t cert[0x1E8];
};

extern struct certz_t certz[51];

#endif // certz_h
