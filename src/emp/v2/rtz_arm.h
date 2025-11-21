#ifndef rtz_arm_h
#define rtz_arm_h

int rtz_arm_set_com_speed(struct sp_port *port, int baudrate);
int rtz_arm_init_boot(struct sp_port *port);
int rtz_arm_send_bootloader(struct sp_port *port, const char *fname);
int rtz_arm_get_flash_id(struct sp_port *port);
int rtz_arm_flash(struct sp_port *port, const char *fw_name);

#endif // rtz_arm_h
