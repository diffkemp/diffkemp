struct Config {
    int base;
    int flag;
    int level;
};

int foo();
int bar();
int foobar(int flag);

int change_struct(struct Config *config) {
    return foobar(config->flag);
}
