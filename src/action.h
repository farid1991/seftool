#ifndef se_h
#define se_h

#include <stdint.h>

typedef enum
{
    ACT_NONE,
    ACT_IDENTIFY,
    ACT_UNLOCK,
    ACT_FLASH,
    ACT_READ_FLASH,
    ACT_READ_GDFS,
    ACT_WRITE_GDFS,
    ACT_WRITE_SCRIPT,
    ACT_CONVERT,
} action_t;

action_t action_from_string(const char *a);

int action_unlock_usercode(struct sp_port *port, struct phone_info *phone);
int action_identify(struct sp_port *port, struct phone_info *phone);
int action_flash_fw(struct sp_port *port, struct phone_info *phone, const char *main_fw, const char *fs_fw);
int action_read_flash(struct sp_port *port, struct phone_info *phone, uint32_t addr, uint32_t size);
int action_backup_gdfs(struct sp_port *port, struct phone_info *phone);
int action_restore_gdfs(struct sp_port *port, struct phone_info *phone, const char *inputfname);
int action_exec_scripts(struct sp_port *port, struct phone_info *phone,
                        int nfiles, const char **filenames);
int action_convert(const char *cnv_mode, const char *cnv_filename, uint32_t mem_addr);

#endif // se_h
