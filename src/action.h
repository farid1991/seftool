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
    ACT_UPLOAD_FS,
    ACT_DOWNLOAD_FS,
    ACT_UPLOAD_ANYCID,
    ACT_FLASH_AVR,
    ACT_FLASH_ARM,
} action_t;

action_t action_from_string(const char *a);

int action_unlock_usercode(struct sp_port *port, struct phone_info *phone);
int action_identify(struct sp_port *port, struct phone_info *phone);
int action_flash_fw(struct sp_port *port, struct phone_info *phone,
                    const char *main_fw, const char *fs_fw, const char *cda);
int action_read_flash(struct sp_port *port, struct phone_info *phone, uint32_t addr, uint32_t size);
int action_backup_gdfs(struct sp_port *port, struct phone_info *phone);
int action_restore_gdfs(struct sp_port *port, struct phone_info *phone, const char *inputfname);
int action_exec_scripts(struct sp_port *port, struct phone_info *phone,
                        int nfiles, const char **filenames);
int action_convert(const char *cnv_mode, const char *cnv_filename, uint32_t mem_addr);
int action_upload_to_fs(struct sp_port *port, struct phone_info *phone,
                        const char **src_files, int src_count, const char *dst_dir);
int action_download_from_fs(struct sp_port *port, struct phone_info *phone,
                            const char *src_dir,
                            const char *dest_dir);
int action_upload_anycid(struct sp_port *port, struct phone_info *phone);

#endif // se_h
