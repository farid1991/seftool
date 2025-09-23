#ifndef flash_h
#define flash_h

#include <stdint.h>

int flash_read(struct sp_port *port, struct phone_info *phone,
               uint32_t memaddr, int size);

int flash_babe(struct sp_port *port, uint8_t *addr, long size, int flashfull);
int flash_babe_fw(struct sp_port *port, const char *filename, int flashfull);

#endif // flash_h