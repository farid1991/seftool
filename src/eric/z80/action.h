#ifndef eric_z80_action_h
#define eric_z80_action_h

int action_z80_identify(struct sp_port *port, struct phone_info *phone);
int action_z80_read_eeprom(struct sp_port *port, struct phone_info *phone);
int action_z80_write_eeprom(struct sp_port *port, struct phone_info *phone, const char *eeprom);
int action_z80_unlock_usercode(struct sp_port *port, struct phone_info *phone);

#endif // eric_z80_action_h