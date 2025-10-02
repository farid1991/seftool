#ifndef connection_h
#define connection_h

int connection_open(struct sp_port *port, struct phone_info *phone);
int connection_close(struct sp_port *port);

#endif // connection_h