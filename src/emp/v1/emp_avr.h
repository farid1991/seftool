#ifndef emp_avr_h
#define emp_avr_h

int emp_avr_bootup(struct sp_port *port, struct phone_info *phone);
int emp_avr_send_flashloader(struct sp_port *port, struct phone_info *phone);

#endif // emp_avr_h
