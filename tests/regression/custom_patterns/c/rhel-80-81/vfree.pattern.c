struct vm_struct *remove_vm_area();
void vm_remove_mappings();

#define PATTERN_NAME __vunmap
#define PATTERN_ARGS                                                           \
    const void *addr, int deallocate_pages, struct vm_struct *area

PATTERN_OLD { remove_vm_area(addr); }

PATTERN_NEW { vm_remove_mappings(area, deallocate_pages); }
