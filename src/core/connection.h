#ifndef connection_h
#define connection_h

int connection_open(struct sp_port *port, struct phone_info *phone);
int connection_close(struct sp_port *port);
int connection_restart(struct sp_port *port, struct phone_info *phone);
int connection_set_speed(struct sp_port *port, struct phone_info *phone);
int connection_get_type(struct sp_port *port, struct phone_info *phone);

int wait_for_Z(struct sp_port *port, struct phone_info *phone);
int send_question_mark(struct sp_port *port, struct phone_info *phone);

#endif // connection_h