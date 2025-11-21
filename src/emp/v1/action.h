#ifndef action_emp_v1_h
#define action_emp_v1_h

int emp_avr_action_identify(struct sp_port *port, struct phone_info *phone);
int emp_avr_action_flash_sbn(struct sp_port *port, struct phone_info *phone, const char *fname);

#endif // action_emp_v1_h
