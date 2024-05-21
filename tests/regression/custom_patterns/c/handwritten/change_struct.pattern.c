// test manually with `--disable-pattern "struct-alignment"` for a proper test

struct Config {
    int base;
    int flag;
    int level;
};

struct NewConfig {
    int base;
    int level;
    int flag;
};

int FUNCTION_OLD(change_struct_flag, struct Config *config, struct NewConfig *new_config) {
    return config->flag;
}
int FUNCTION_NEW(change_struct_flag, struct Config *config, struct NewConfig *new_config) {
    return new_config->flag;
}

int FUNCTION_OLD(change_struct_level, struct Config *config, struct NewConfig *new_config) {
    return config->level;
}
int FUNCTION_NEW(change_struct_level, struct Config *config, struct NewConfig *new_config) {
    return new_config->level;
}
