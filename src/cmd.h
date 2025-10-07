#ifndef cmd_h
#define cmd_h

#include <stdint.h>

// PRODID_LDR MAIN_CMD
#define CMD_GET_FLASHID         0x0D
#define CMD_SHUTDOWN            0x14

// CS_LDR MAIN_CMD
#define CMD_CSLOADER            0x01
#define CMD_CS_FSX              0x03
#define CMD_CS_GDFS             0x04
#define CMD_CS_SHUTDOWN         0x08
#define CMD_CS_RESETLOCK        0x0D

// CS_LDR SUB_CMD CMD_CS_GDFS
#define CMD_GDFS_READVAR        0x01
#define CMD_GDFS_READALL        0x02
#define CMD_GDFS_WRITEVAR       0x03
#define CMD_GDFS_ACTIVATION     0x05

// SUB_CMD CMD_CS_FSX
// #define CMD_FSX_COPYFILE      0x01
#define CMD_FSX_RMFILE          0x04
#define CMD_FSX_STAT            0x06
#define CMD_FSX_MKDIR           0x07
#define CMD_FSX_CHDIR           0x08
#define CMD_FSX_RMDIR           0x09
#define CMD_FSX_LISTFILES       0x0A
#define CMD_FSX_LISTDIRS        0x0B
#define CMD_FSX_PUTFILE         0x0C
#define CMD_FSX_GETFILE         0x0D
#define CMD_FSX_GETDIR          0x10

struct packetdata_t
{
    uint8_t ack;
    uint8_t hdr;
    uint8_t cmd;
    uint16_t length;
    uint8_t data[1024];
    uint8_t checksum;
};

int cmd_encode_binary_packet(uint8_t cmd, const uint8_t *data, int datasize, uint8_t *outbuf);
int cmd_encode_read_gdfs(uint8_t block, uint8_t lsb, uint8_t msb, uint8_t *outbuf);
int cmd_encode_csloader_packet(uint8_t cmd, uint8_t subcmd,
                               const uint8_t *csdata, int datasize,
                               uint8_t *outbuf);

int cmd_decode_packet_ack(const uint8_t *buf, int size, struct packetdata_t *out);
int cmd_decode_packet_noack(const uint8_t *buf, int size, struct packetdata_t *out);
int cmd_decode_packet(const uint8_t *buf, int size, struct packetdata_t *out);

#endif // cmd_h
