#ifndef se_h
#define se_h

#include <stdint.h>
#include <cli_args.h>

#include <emp/v1/action.h>
#include <emp/v2/action.h>
#include <emp/v3/action.h>
#include <eric/avr/action.h>
#include <eric/z80/action.h>

typedef enum {
	ACT_NONE,
	ACT_IDENTIFY,
	ACT_UNLOCK,
	ACT_FLASH,
	ACT_READ_FLASH,
	ACT_READ_GDFS,
	ACT_WRITE_GDFS,
	ACT_WRITE_SCRIPT,
	ACT_CONVERT,
	ACT_UPLOAD_FS,
	ACT_DOWNLOAD_FS,
	ACT_UPLOAD_ANYCID,
	ACT_UNLOCK_AVR,
	ACT_FLASH_AVR,
	ACT_READ_AVR,
	ACT_FLASH_ARM,
	ACT_READ_ARM,
	ACT_FLASH_Z80,
	ACT_READ_Z80,
	ACT_READ_EEPROM,
	ACT_WRITE_EEPROM,
} action_t;

action_t action_from_string(const char *a);
void action_print_summary(action_t act, const cli_args *args);
int action_phone_avr(struct sp_port *port, struct phone_info *phone, action_t act, cli_args *args);
int action_phone_z80(struct sp_port *port, struct phone_info *phone, action_t act, cli_args *args);
int action_emp_v1(struct sp_port *port, struct phone_info *phone, action_t act, cli_args *args);
int action_emp_v2(struct sp_port *port, struct phone_info *phone, action_t act, cli_args *args);
int action_emp_v3(struct sp_port *port, struct phone_info *phone, action_t act, cli_args *args);

#endif // se_h
