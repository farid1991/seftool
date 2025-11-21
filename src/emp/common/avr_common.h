#ifndef avr_common_h
#define avr_common_h

int emp_avr_bypass_boot_authority(struct sp_port *port, struct phone_info *phone);

int emp_avr_init_cmd(struct sp_port *port);
int emp_avr_queue_cmd(struct sp_port *port);
int emp_avr_next_cmd(struct sp_port *port);
void emp_avr_finish(struct sp_port *port);

int emp_avr_set_baudrate(struct sp_port *port, int baudrate);
int emp_avr_send_loader(struct sp_port *port, const char *dirname, const char *fname);

int emp_avr_get_imei(struct sp_port *port, struct phone_info *phone);
int emp_avr_get_gdfs_imei(struct sp_port *port, struct phone_info *phone);
int emp_avr_get_ldr_v(struct sp_port *port);
const char *emp_avr_get_gdfsloader(const char *device_name);

int emp_avr_flash_sbn(struct sp_port *port, const char *fname);

int avr_gdfs_resp(struct sp_port *port, uint8_t ack);
int getgdfs_44(struct sp_port *port);
int getgdfs_45(struct sp_port *port);
int avr_f_resetusercode(struct sp_port *port);

#endif // avr_common_h
