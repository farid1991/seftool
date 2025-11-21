#ifndef eric_avr_t28_h
#define eric_avr_t28_h

int eric_t28_identify(struct sp_port *port, struct phone_info *phone);
int eric_t28_read_eeprom(struct sp_port *port, struct phone_info *phone);
int eric_t28_write_eeprom(struct sp_port *port, struct phone_info *phone, const char *eep_name);

#endif // eric_avr_t28_h
