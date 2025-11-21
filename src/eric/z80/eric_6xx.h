#ifndef eric_6xx_h
#define eric_6xx_h

// 6XX & 7XX
int eric_6xx_identify(struct sp_port *port, struct phone_info *phone);
int eric_6xx_read_eeprom(struct sp_port *port, struct phone_info *phone);
int eric_6xx_write_eeprom(struct sp_port *port, struct phone_info *phone, const char *eeprom);
int eric_6xx_unlock_usercode(struct sp_port *port, struct phone_info *phone);

#endif // eric_6xx_h
