#ifndef gdfs_h
#define gdfs_h

struct gdfs_data_t
{
    char phone_name[32];
    char brand[32];
    char cxc_article[32];
    char cxc_version[32];
    char langpack[32];
    char cda_article[32];
    char cda_revision[32];
    char default_article[32];
    char default_version[32];
    char user_lock[16];
    char mcc[16];
    char mnc[16];
    uint8_t locked;
};

enum 
{
    GD_PHONE_NAME,
    GD_BRAND,
    GD_CXC_ARTICLE,
    GD_CXC_VERSION,
    GD_LANGPACK,
    GD_CDA_ARTICLE,
    GD_CDA_REVISION,
    GD_DEF_ARTICLE,
    GD_DEF_VERSION,
    GD_COUNT,
};

int gdfs_get_phonename(struct sp_port *port, struct phone_info *phone, struct gdfs_data_t *gdfs);
int gdfs_get_brand(struct sp_port *port, struct phone_info *phone, struct gdfs_data_t *gdfs);
int gdfs_get_cxc_article(struct sp_port *port, struct phone_info *phone, struct gdfs_data_t *gdfs);
int gdfs_get_cxc_version(struct sp_port *port, struct phone_info *phone, struct gdfs_data_t *gdfs);
int gdfs_get_language(struct sp_port *port, struct phone_info *phone, struct gdfs_data_t *gdfs);
int gdfs_get_cda_article(struct sp_port *port, struct phone_info *phone, struct gdfs_data_t *gdfs);
int gdfs_get_cda_revision(struct sp_port *port, struct phone_info *phone, struct gdfs_data_t *gdfs);
int gdfs_get_default_article(struct sp_port *port, struct phone_info *phone, struct gdfs_data_t *gdfs);
int gdfs_get_default_version(struct sp_port *port, struct phone_info *phone, struct gdfs_data_t *gdfs);
int gdfs_get_simlock(struct sp_port *port, struct gdfs_data_t *gdfs);
int gdfs_parse_simlockdata(struct gdfs_data_t *gdfs, uint8_t *simlock);
int gdfs_get_userlock(struct sp_port *port, struct gdfs_data_t *gdfs);
int gdfs_unlock_usercode(struct sp_port *port);
int gdfs_dump_sec_units(struct sp_port *port, struct phone_info *phone, const char *backup_name);
int gdfs_terminate_access(struct sp_port *port);


#endif // gdfs_h