#ifndef serial_h
#define serial_h

#include <stdint.h>

int serial_open(struct sp_port *port);
int serial_set_baudrate(struct sp_port *port, int baudrate);

// --- Write helpers ---
int serial_write(struct sp_port *port, const uint8_t *buf, size_t len);
int serial_write_chunks(struct sp_port *port, const uint8_t *buf, size_t len, size_t chunk_size);
int serial_send_packetdata_ack(struct sp_port *port, const uint8_t *data, size_t len);
int serial_send_ack(struct sp_port *port);

// --- Read helpers ---
int serial_read(struct sp_port *port, uint8_t *buf, size_t bufsize, int timeout_ms);
int serial_wait_ack(struct sp_port *port, int timeout_ms);
int serial_wait_packet(struct sp_port *port, uint8_t *buf, size_t bufsize, int timeout_ms);
int serial_wait_e3_answer(struct sp_port *port, const char *expected, int timeout_ms, int skiperrors);

#endif // serial_h
