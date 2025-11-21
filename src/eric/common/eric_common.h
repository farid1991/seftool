#ifndef eric_common_h
#define eric_common_h

const char *eric_get_speed_val(int speed);
int eric_new_handshake(struct sp_port *port);
int eric_init_cmd(struct sp_port *port);

#endif // eric_common_h
