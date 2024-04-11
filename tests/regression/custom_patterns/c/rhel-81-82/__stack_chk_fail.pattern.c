extern int suppress_printk;
void console_flush_on_panic();
void panic_print_sys_info();

#define PATTERN_NAME panic_first
#define PATTERN_ARGS

PATTERN_OLD { console_flush_on_panic(); }

PATTERN_NEW {
    console_flush_on_panic(0);
    panic_print_sys_info();
}

#define PATTERN_NAME panic_second
#define PATTERN_ARGS

PATTERN_OLD {}

PATTERN_NEW { suppress_printk = 1; }
