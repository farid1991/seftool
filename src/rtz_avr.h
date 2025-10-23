#ifndef rtz_avr_h
#define rtz_avr_h

#define AVRCOMSPEED115200 0x17
#define AVRCOMSPEED230400 0x15
#define AVRCOMSPEED460800 0x13
#define AVRCOMSPEED921600 0x11

typedef struct
{
    uint8_t unk_1[6];
    uint16_t cid;
    uint32_t color;
    uint8_t cert[0x1AB];
    uint32_t prologue_size0;
    uint32_t prologue_size1;
    uint8_t unk_2[0xB8];
    uint32_t payload_size0;
    uint32_t payload_size1;
    uint8_t unk_3[0x80];
} avr_bin_t;

#pragma pack(push, 1)
typedef struct
{
    uint16_t MfrId;
    uint16_t DevId;
    uint8_t __unknown[12];
    char QRY[3];
    uint16_t PrimVendorCmdCtrlID;
    uint16_t PrimAlgQTBL;
    uint16_t AltVendorCmdCtrlID;
    uint16_t AltAlgQTBL;
    uint8_t VCCLogicSupply_minimum_wrer_voltage;
    uint8_t VCCLogicSupply_maximum_wrer_voltage;
    uint8_t VCCProgrammingSupply_minimum_wrer_voltage;
    uint8_t VCCProgrammingSupply_maximum_wrer_voltage;
    uint8_t typicaltimeoutpersinglechar;
    uint8_t typicaltimeoutpermaxbuff;
    uint8_t typicaltimeout_1blockerase;
    uint8_t typicaltimeout_fullerase;
    uint8_t maxtimeoutpersinglechar;
    uint8_t maxtimeoutpermaxbuff;
    uint8_t maxtimeout_1blockerase;
    uint8_t maxtimeout_fullerase;
    uint8_t DeviceSize; // 1 << n bytes
    uint16_t DeviceInterfaceDescription;
    uint16_t MaxBytesInMultiWrite;
    uint8_t NumberOfEraseBlockRegions;
    uint8_t eraseblockregions[211];
} CFI;
#pragma pack(pop)

typedef struct
{
    uint32_t address;
    uint32_t blocksize;
    uint32_t blocks;
    uint32_t algorithm;
} AVRFlashEBInfo;

uint8_t avr_get_speed_val(int speed);
int rtz_avr_set_com_speed(struct sp_port *port, int baudrate);
int rtz_avr_get_otp_imei(struct sp_port *port, struct phone_info *phone);
int rtz_avr_activate_loader(struct sp_port *port, struct phone_info *phone);
int rtz_avr_send_bootloader(struct sp_port *port);
int rtz_avr_get_flash_info(struct sp_port *port);
int rtz_avr_flash_bin(struct sp_port *port, uint32_t avrbase, const char *firmware);
const char *get_flash_name(uint16_t flashid);

#endif // rtz_avr_h