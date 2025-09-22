#ifndef cmd_h
#define cmd_h

#include <stdint.h>

struct packetdata_t
{
    uint8_t ack;
    uint8_t hdr;
    uint8_t cmd;
    uint16_t length;
    uint8_t data[512];
    uint8_t checksum;
};

// --- cmd_encode_binary_packet ---
// cmd      = command ID
// data     = payload data
// datasize = payload size
// outbuf   = buffer to hold the generated command
// returns number of bytes written
int cmd_encode_binary_packet(uint8_t cmd, const uint8_t *data, int datasize, uint8_t *outbuf);

int cmd_encode_read_gdfs(uint8_t block, uint8_t lsb, uint8_t msb, uint8_t *outbuf);

int cmd_encode_csloader_packet(uint8_t cmd, uint8_t subcmd,
                               const uint8_t *csdata, int datasize,
                               uint8_t *outbuf);

int cmd_decode_packet_ack(const uint8_t *buf, int size, struct packetdata_t *out);
int cmd_decode_packet_noack(const uint8_t *buf, int size, struct packetdata_t *out);
int cmd_decode_packet(const uint8_t *buf, int size, struct packetdata_t *out);

#endif // cmd_h
