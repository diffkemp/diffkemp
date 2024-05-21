struct Config {
    int base;
    int level;
    int flag;
};

int foo();
int bar();
int foobar(int flag);

int change_struct(struct Config *config) {
    return foobar(config->flag);
}
