"""
LLVM module containing definitions of kernel sysctl options compiled into
LLVM IR. Contains reference to LlvmModule and additional sysctl-specific fields
and methods.
"""
from diffkemp.llvm_ir.llvm_module import LlvmParam
from diffkemp.simpll.library import SimpLLSysctlTable


def matches(name, pattern):
    if pattern == "*":
        return True
    if pattern.startswith("{") and pattern.endswith("}"):
        match_list = pattern[1:-1].split("|")
        return name in match_list
    return name == pattern


class LlvmSysctlModule:
    # List of standard proc handler functions.
    standard_proc_funs = {
        "proc_dostring", "proc_dointvec", "proc_douintvec",
        "proc_dointvec_minmax", "proc_douintvec_minmax",
        "proc_dopipe_max_size", "proc_dointvec_jiffies",
        "proc_dointvec_userhz_jiffies", "proc_dointvec_ms_jiffies",
        "proc_dointvec_ms_jiffies", "proc_doulongvec_ms_jiffies_minmax",
        "proc_do_large_bitmap"
    }

    @staticmethod
    def is_standard_proc_fun(proc_fun):
        """Check if the proc handler function is a standard one."""
        return proc_fun in LlvmSysctlModule.standard_proc_funs

    def __init__(self, kernel_mod, ctl_table):
        """
        :param kernel_mod: Reference to LlvmKernelModule with the compiled mod
        :param ctl_table: Name of global variable holding table with sysctl
                          option definitions.
        """
        kernel_mod.parse_module()
        self.mod = kernel_mod
        self.ctl_table = ctl_table
        self.table_object = SimpLLSysctlTable(self.mod.llvm_module, ctl_table)

    def parse_sysctls(self, sysctl_pattern):
        """
        Parse all sysctls entries that match the given pattern. Parsed entries
        are LLVM objects of type "struct ctl_table" containing the sysctl
        definition.
        :return: List of names of parsed sysctls.
        """
        return self.table_object.parse_sysctls(sysctl_pattern)

    def get_proc_fun(self, sysctl_name):
        """
        Get the name of the proc handler function for the given sysctl option.
        """
        fun = self.table_object.get_proc_fun(sysctl_name)
        return fun.get_name() if fun else None

    def get_child(self, sysctl_name):
        """Get the name of the child node of the given sysctl table entry."""
        result = self.table_object.get_child(sysctl_name)
        return (LlvmParam(result[0], result[1])
                if result is not None else None)

    def get_data(self, sysctl_name):
        """Get the name of the data variable for the given sysctl option."""
        result = self.table_object.get_data(sysctl_name)
        return (LlvmParam(result[0], result[1])
                if result is not None else None)
