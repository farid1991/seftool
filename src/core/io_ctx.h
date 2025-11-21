#ifndef io_ctx_h
#define io_ctx_h

#include "serial.h"

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
int wait_for_answer(struct sp_port *port, const char *expected, size_t len, int skiperrors);

void flush(struct sp_port *port);

#endif // io_ctx_h
