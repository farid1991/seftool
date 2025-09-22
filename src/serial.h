#ifndef serial_h
#define serial_h

#include <stdint.h>

// --- Write helpers ---
int serial_write(struct sp_port *port, const uint8_t *buf, int len);
int serial_write_chunks(struct sp_port *port, const uint8_t *buf, int len, int chunk_size);

int serial_send_packetdata_ack(struct sp_port *port, const uint8_t *data, size_t len);

int serial_send_ack(struct sp_port *port);
int serial_wait_ack(struct sp_port *port, int timeout_ms);
int serial_wait_packet(struct sp_port *port, uint8_t *buf, size_t bufsize, int timeout_ms);

#endif // serial_h
