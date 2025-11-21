#ifndef eric_8xx_h
#define eric_8xx_h

// 868 & 888
int eric_8xx_identify(struct sp_port *port, struct phone_info *phone);
int eric_8xx_read_eeprom(struct sp_port *port, struct phone_info *phone);
int eric_8xx_write_eeprom(struct sp_port *port, struct phone_info *phone, const char *eeprom);
int eric_8xx_unlock_usercode(struct sp_port *port, struct phone_info *phone);
int eric_8xx_change_imei(struct sp_port *port, struct phone_info *phone);

#endif // eric_8xx_h
