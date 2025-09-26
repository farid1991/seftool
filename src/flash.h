#ifndef flash_h
#define flash_h

#include <stdint.h>

int flash_detect_fw_version(struct sp_port *port, struct phone_info *phone);

int flash_read(struct sp_port *port, struct phone_info *phone,
               uint32_t memaddr, int size);

int flash_babe(struct sp_port *port, uint8_t *addr, long size, int flashfull);
int flash_babe_fw(struct sp_port *port, const char *filename, int flashfull);

int flash_raw(struct sp_port *port, const char *filename, uint32_t rawaddr);

int flash_restore_boot_area(struct sp_port *port, struct phone_info *phone);

#endif // flash_h