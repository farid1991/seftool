#ifndef eric_avr_action_h
#define eric_avr_action_h

int action_avr_identify(struct sp_port *port, struct phone_info *phone);
int action_avr_read_eeprom(struct sp_port *port, struct phone_info *phone);
int action_avr_write_eeprom(struct sp_port *port, struct phone_info *phone, const char *eeprom);
int action_avr_unlock_usercode(struct sp_port *port, struct phone_info *phone);

#endif // eric_avr_action_h
