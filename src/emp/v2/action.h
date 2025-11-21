#ifndef emp_v2_action_h
#define emp_v2_action_h

int action_rtz_identify(struct sp_port *port, struct phone_info *phone);
int action_rtz_flash_arm(struct sp_port *port, struct phone_info *phone, const char *fw_name);
int action_rtz_flash_avr(struct sp_port *port, struct phone_info *phone, const char *fw_name);
int action_rtz_read_avr(struct sp_port *port, struct phone_info *phone, uint32_t addr, uint32_t size);
int action_rtz_unlock_usercode(struct sp_port *port, struct phone_info *phone, const char *device_name);

#endif // emp_v2_action_h
