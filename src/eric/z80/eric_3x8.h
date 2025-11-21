#ifndef eric_3x8_h
#define eric_3x8_h

// 318 & 388
int eric_3x8_identify(struct sp_port *port, struct phone_info *phone);
int eric_3x8_read_eeprom(struct sp_port *port, struct phone_info *phone);
int eric_3x8_write_eeprom(struct sp_port *port, struct phone_info *phone, const char *eeprom);
int eric_3x8_unlock_usercode(struct sp_port *port, struct phone_info *phone);

#endif // eric_3x8_h
