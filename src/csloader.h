#ifndef csloader_h
#define csloader_h

#include <stdint.h>

int csloader_write_gdfs_var(struct sp_port *port, uint8_t block, uint8_t lo, uint8_t hi, uint8_t *data, uint32_t size);
int csloader_write_gdfs(struct sp_port *port, const char *inputfname);
int csloader_read_gdfs(struct sp_port *port, struct phone_info *phone);
int csloader_parse_gdfs_script(struct sp_port *port, const char *inputfname, const char *outputfname);

#endif // csloader_h
