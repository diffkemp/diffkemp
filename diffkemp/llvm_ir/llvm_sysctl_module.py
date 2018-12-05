"""
Kernel module containing definitions of sysctl options compiled into LLVM IR.
Contains reference to LlvmKernelModule and additional sysctl-specific fields
and methods.
"""
from llvmcpy.llvm import *


class LlvmSysctlModule:
    # List of standard proc handler functions.
    standard_proc_funs = [
        "proc_dostring", "proc_dointvec", "proc_douintvec",
        "proc_dointvec_minmax", "proc_douintvec_minmax",
        "proc_dopipe_max_size", "proc_dointvec_jiffies",
        "proc_dointvec_userhz_jiffies", "proc_dointvec_ms_jiffies",
        "proc_dointvec_ms_jiffies", "proc_doulongvec_ms_jiffies_minmax",
        "proc_do_large_bitmap"
    ]

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
        self.mod = kernel_mod
        self.ctl_table = ctl_table
        self.sysctls = dict()

    def _get_sysctl(self, sysctl_name):
        """
        Get LLVM object (of type struct ctl_table) with the definition of
        the given sysctl option.
        """
        # If the given table entry has been already found, return it.
        if sysctl_name in self.sysctls:
            return self.sysctls[sysctl_name]

        self.mod.parse_module()
        ctl_table = self.ctl_table.split(".")
        # sysctl table is a global variable
        table = self.mod.llvm_module.get_named_global(ctl_table[0])
        if not table:
            return None

        # Get global variable initializer. If sysctl_name contains some indices
        # follow them to get the actual table.
        sysctl_list = table.get_initializer()
        for i in ctl_table[1:]:
            sysctl_list = sysctl_list.get_operand(int(i))
        if not sysctl_list:
            return None

        # Iterate all entries in the table
        for i in range(0, sysctl_list.get_num_operands()):
            sysctl = sysctl_list.get_operand(i)
            if sysctl.get_num_operands() == 0:
                continue
            size = ffi.new("size_t *")
            # Sysctl option name is the first element of the entry
            # We need one more .get_operand(0) because of gep operator
            name = sysctl.get_operand(0).get_operand(0).get_initializer() \
                .get_as_string(size)
            if name == sysctl_name.split(".")[-1]:
                self.sysctls[sysctl_name] = sysctl
                return sysctl

    def get_proc_fun(self, sysctl_name):
        """
        Get name of the proc handler function for the given sysctl option.
        """
        sysctl = self._get_sysctl(sysctl_name)
        if not sysctl or sysctl.get_num_operands() < 6:
            return None
        # Proc handler function is the 6th element of the
        # "struct ctl_table" type.
        proc_handler = sysctl.get_operand(5)
        return proc_handler.get_name() if not proc_handler.is_null() else None

    def get_data(self, sysctl_name):
        """Get name of the data variable for the given sysctl option."""
        sysctl = self._get_sysctl(sysctl_name)
        if not sysctl or sysctl.get_num_operands() < 2:
            return None
        # Address of the data variable is the 2nd element of the
        # "struct ctl_table" type.
        data = sysctl.get_operand(1)
        if data.is_null():
            return None
        if data.get_kind() == ConstantExprValueKind:
            if data.get_const_opcode() == Opcode['GetElementPtr']:
                # Address is a GEP, we have to extract the actual variable.
                all_constant = True
                indices = list()
                # Look whether are all indices constant.
                for i in range(1, data.get_num_operands()):
                    indices.append(data.get_operand(i))
                    if not data.get_operand(i).is_constant():
                        all_constant = False
                if all_constant:
                    data = data.get_operand(0)
                    if data.get_const_opcode() == Opcode['BitCast']:
                        data = data.get_operand(0)
            elif data.get_const_opcode() == Opcode['BitCast']:
                # Address is typed to "void *", we need to get rid of the
                # bitcast operator.
                data = data.get_operand(0)
        if data.get_kind() == GlobalVariableValueKind:
            return data.get_name()
        return None
