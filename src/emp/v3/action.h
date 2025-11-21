#ifndef emp_v3_action_h
#define emp_v3_action_h

int action_unlock_usercode(struct sp_port *port, struct phone_info *phone);
int action_identify(struct sp_port *port, struct phone_info *phone);
int action_flash_fw(struct sp_port *port, struct phone_info *phone, const char *main_fw, const char *fs_fw,
                    const char *cda);
int action_read_flash(struct sp_port *port, struct phone_info *phone, uint32_t addr, uint32_t size);
int action_backup_gdfs(struct sp_port *port, struct phone_info *phone);
int action_restore_gdfs(struct sp_port *port, struct phone_info *phone, const char *inputfname);
int action_exec_scripts(struct sp_port *port, struct phone_info *phone, int nfiles, const char **filenames);
int action_convert(const char *cnv_mode, const char *cnv_filename, uint32_t mem_addr);
int action_upload_to_fs(struct sp_port *port, struct phone_info *phone, const char **src_files, int src_count,
                        const char *dst_dir);
int action_download_from_fs(struct sp_port *port, struct phone_info *phone, const char *src_dir, const char *dest_dir);
int action_upload_anycid(struct sp_port *port, struct phone_info *phone);

#endif // emp_v3_action_h