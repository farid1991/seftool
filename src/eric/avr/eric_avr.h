#ifndef eric_avr_h
#define eric_avr_h

int eric_avr_set_speed(struct sp_port *port, int baudrate);
int eric_avr_send_byte(struct sp_port *port, uint8_t cmd);
int eric_avr_send_loader(struct sp_port *port, const char *fname);
int eric_avr_get_imei(struct sp_port *port, struct phone_info *phone);

#endif // eric_avr_h
