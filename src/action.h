#ifndef se_h
#define se_h

#include <stdint.h>

typedef enum
{
    ACT_NONE,
    ACT_IDENTIFY,
    ACT_UNLOCK_USERCODE,
    ACT_UNLOCK_SIMLOCK,
    ACT_FLASH,
    ACT_READ_FLASH,
    ACT_READ_GDFS,
    ACT_WRITE_GDFS
} action_t;

int action_unlock_usercode(struct sp_port *port, struct phone_info *phone);
int action_identify(struct sp_port *port, struct phone_info *phone);
int action_flash_fw(struct sp_port *port, struct phone_info *phone, const char *main_fw, const char *fs_fw);
int action_read_flash(struct sp_port *port, struct phone_info *phone, uint32_t addr, uint32_t size);
int action_backup_gdfs(struct sp_port *port, struct phone_info *phone);
int action_restore_gdfs(struct sp_port *port, struct phone_info *phone, const char *inputfname);
int action_exec_script(struct sp_port *port, struct phone_info *phone, const char *inputfname);

#endif // se_h
