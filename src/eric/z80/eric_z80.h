#ifndef eric_z80_h
#define eric_z80_h

enum eric_loader_type_e {
	LOADER_VERS,
	FIRMWARE_VERS,
};

int eric_z80_send_byte(struct sp_port *port, uint8_t cmd);
int eric_z80_send_loader(struct sp_port *port, const char *fname);
int eric_z80_set_speed(struct sp_port *port, int baudrate);
int eric_z80_get_ldr_version(struct sp_port *port, struct phone_info *phone);
int eric_z80_get_vers(struct sp_port *port, struct phone_info *phone, int version);
int eric_z80_get_eep_imei(struct sp_port *port, struct phone_info *phone);
int eric_z80_get_baudrate(struct sp_port *port);
int eric_z80_set_baudrate(struct sp_port *port, int baudrate);
void eric_z80_terminate(struct sp_port *port);

int eric_parse_response(const char *resp, const char *cmd, char *out, size_t out_size);

int eric_write_eeprom_byte(struct sp_port *port, uint16_t base, uint16_t addr, uint8_t value);
int eric_read_eeprom_byte(struct sp_port *port, uint16_t base, uint16_t addr, uint8_t *value);
int eric_read_eeprom_block(struct sp_port *port, uint16_t base, uint16_t addr, int read_size, uint8_t *out);
int eric_read_eeprom(struct sp_port *port, const char *filename, uint16_t base, size_t eeprom_size);
int eric_write_eeprom(struct sp_port *port, const char *filename, uint16_t base);

void eric_decode_splock(struct sp_port *port, uint16_t base, uint16_t addr, char *out);
void eric_decode_phonelock(struct sp_port *port, uint16_t base, uint16_t addr, char *out);
int eric_change_imei(struct sp_port *port, uint16_t base, uint16_t offset);
int eric_get_imei_data(struct sp_port *port, struct phone_info *phone, uint16_t base, uint16_t addr);

#endif // eric_z80_h
