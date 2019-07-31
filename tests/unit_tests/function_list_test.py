"""Unit tests for the FunctionList class."""

from diffkemp.function_list import FunctionList
from diffkemp.llvm_ir.kernel_module import LlvmKernelModule
import yaml


def test_create_list_no_groups():
    """
    Create function list with a single "None" group.
    Add a function into the list and check that it is there.
    """
    lst = FunctionList("snapshots/linux-3.10.0-957.el7")
    assert lst.root_dir == "snapshots/linux-3.10.0-957.el7"
    assert lst.kind is None

    lst.add_none_group()
    assert len(lst.groups) == 1
    assert None in lst.groups

    lst.add("___pskb_trim", LlvmKernelModule("net/core/skbuff.ll"))
    assert "___pskb_trim" in lst.groups[None].functions
    fun_desc = lst.groups[None].functions["___pskb_trim"]
    assert fun_desc.mod.llvm == "net/core/skbuff.ll"
    assert fun_desc.glob_var is None
    assert fun_desc.tag is None


def test_create_list_sysctl_group():
    """
    Create function list with a named group. Sysctl option is used as
    group name since this is the normal use case.
    Add function to the group and check that it is there.
    :return:
    """
    lst = FunctionList("snapshots-sysctl/linux-3.10.0-957.el7", "sysctl")
    assert lst.root_dir == "snapshots-sysctl/linux-3.10.0-957.el7"
    assert lst.kind == "sysctl"

    lst.add_group("kernel.sched_latency_ns")
    assert len(lst.groups) == 1
    assert "kernel.sched_latency_ns" in lst.groups

    lst.add("sched_debug_header",
            LlvmKernelModule("kernel/sched/debug.ll"),
            "sysctl_sched_latency",
            "using_data_variable \"sysctl_sched_latency\"",
            "kernel.sched_latency_ns")
    assert "sched_debug_header" in lst.groups[
        "kernel.sched_latency_ns"].functions
    fun_desc = lst.groups["kernel.sched_latency_ns"].functions[
        "sched_debug_header"]
    assert fun_desc.mod.llvm == "kernel/sched/debug.ll"
    assert fun_desc.glob_var == "sysctl_sched_latency"
    assert fun_desc.tag == "using_data_variable \"sysctl_sched_latency\""


def test_get_modules():
    """
    Test getting all modules in the lists.
    Check if the list returns a list of all modules of all groups in case that
    multiple groups are present.
    """
    lst = FunctionList("snapshots-sysctl/linux-3.10.0-957.el7", "sysctl")
    lst.add_group("kernel.sched_latency_ns")
    lst.add_group("kernel.timer_migration")
    lst.add("sched_proc_update_handler",
            LlvmKernelModule("kernel/sched/fair.ll"), None, "proc_handler",
            "kernel.sched_latency_ns")
    lst.add("proc_dointvec_minmax", LlvmKernelModule("kernel/sysctl.ll"), None,
            "proc_handler", "kernel.timer_migration")

    modules = lst.modules()
    assert len(modules) == 2
    assert set([m.llvm for m in modules]) == {"kernel/sched/fair.ll",
                                              "kernel/sysctl.ll"}


def test_filter():
    """Filtering functions."""
    lst = FunctionList("snapshots/linux-3.10.0-957.el7")
    lst.add_none_group()
    lst.add("___pskb_trim", LlvmKernelModule("net/core/skbuff.ll"))
    lst.add("__alloc_pages_nodemask", LlvmKernelModule("mm/page_alloc.ll"))

    lst.filter(["__alloc_pages_nodemask"])
    assert len(lst.groups[None].functions) == 1
    assert "___pskb_trim" not in lst.groups[None].functions
    assert "__alloc_pages_nodemask" in lst.groups[None].functions


def test_to_yaml_functions():
    """
    Dump a list with a single "None" group into YAML.
    YAML string should contain a simple list of functions, each one having
    the "name" and the "llvm" field set.
    The LLVM paths in the YAML should be relative to the function list root.
    """
    lst = FunctionList("snapshots/linux-3.10.0-957.el7")
    lst.add_none_group()
    lst.add("___pskb_trim", LlvmKernelModule(
        "snapshots/linux-3.10.0-957.el7/net/core/skbuff.ll"))
    lst.add("__alloc_pages_nodemask", LlvmKernelModule(
        "snapshots/linux-3.10.0-957.el7/mm/page_alloc.ll"))

    yaml_str = lst.to_yaml()
    functions = yaml.safe_load(yaml_str)
    assert len(functions) == 2
    assert set([f["name"] for f in functions]) == {"___pskb_trim",
                                                   "__alloc_pages_nodemask"}
    for f in functions:
        if f["name"] == "___pskb_trim":
            assert f["llvm"] == "net/core/skbuff.ll"
        elif f["name"] == "__alloc_pages_nodemask":
            assert f["llvm"] == "mm/page_alloc.ll"


def test_to_yaml_sysctls():
    """
    Dump a list with multiple sysctl groups into YAML.
    YAML string should contain a list of groups, each group containing a list
    of functions.
    The LLVM paths in the YAML should be relative to the function list root.
    """
    lst = FunctionList("snapshots-sysctl/linux-3.10.0-957.el7", "sysctl")
    lst.add_group("kernel.sched_latency_ns")
    lst.add_group("kernel.timer_migration")
    lst.add("sched_proc_update_handler",
            LlvmKernelModule(
                "snapshots-sysctl/linux-3.10.0-957.el7/kernel/sched/fair.ll"),
            None, "proc handler", "kernel.sched_latency_ns")
    lst.add("proc_dointvec_minmax",
            LlvmKernelModule(
                "snapshots-sysctl/linux-3.10.0-957.el7/kernel/sysctl.ll"),
            None, "proc handler", "kernel.timer_migration")

    yaml_str = lst.to_yaml()
    groups = yaml.safe_load(yaml_str)
    assert len(groups) == 2
    assert set([g["sysctl"] for g in groups]) == {"kernel.sched_latency_ns",
                                                  "kernel.timer_migration"}
    for g in groups:
        assert len(g["functions"]) == 1
        if g["sysctl"] == "kernel.sched_latency_ns":
            assert g["functions"][0] == {
                "name": "sched_proc_update_handler",
                "llvm": "kernel/sched/fair.ll",
                "glob_var": None,
                "tag": "proc handler"
            }
        elif g["sysctl"] == "kernel.timer_migration":
            assert g["functions"][0] == {
                "name": "proc_dointvec_minmax",
                "llvm": "kernel/sysctl.ll",
                "glob_var": None,
                "tag": "proc handler"
            }


def test_from_yaml_functions():
    """
    Load function list from YAML. Use YAML that contains only a list of
    functions. It is expected that the parsed list contains a single "None"
    group which contains the list of loaded functions.
    The loaded LLVM paths should contain the list root dir.
    """
    lst = FunctionList("snapshots/linux-3.10.0-957.el7")
    lst.from_yaml("""
    - glob_var: null
      llvm: net/core/skbuff.ll
      name: ___pskb_trim
      tag: null
    - glob_var: null
      llvm: mm/page_alloc.ll
      name: __alloc_pages_nodemask
      tag: null
    """)

    assert len(lst.groups) == 1
    assert None in lst.groups
    assert len(lst.groups[None].functions) == 2
    assert set(lst.groups[None].functions.keys()) == {"___pskb_trim",
                                                      "__alloc_pages_nodemask"}
    for name, f in lst.groups[None].functions.items():
        assert f.glob_var is None
        assert f.tag is None
        if name == "___pskb_trim":
            assert f.mod.llvm == \
                   "snapshots/linux-3.10.0-957.el7/net/core/skbuff.ll"
        elif name == "__alloc_pages_nodemas":
            assert f.mod.llvm == \
                   "snapshots/linux-3.10.0-957.el7/mm/page_alloc.ll"


def test_from_yaml_sysctls():
    """
    Load function list from YAML. Use YAML that contains a list of sysctl
    groups, each group containing a single function.
    The loaded LLVM paths should contain the list root dir.
    """
    lst = FunctionList("snapshots-sysctl/linux-3.10.0-957.el7", "sysctl")
    lst.from_yaml("""
    - functions:
        - glob_var: null
          llvm: kernel/sched/fair.ll
          name: sched_proc_update_handler
          tag: proc handler
      sysctl: kernel.sched_latency_ns
    - functions:
        - glob_var: null
          llvm: kernel/sysctl.ll
          name: proc_dointvec_minmax
          tag: proc handler
      sysctl: kernel.timer_migration
    """)

    assert len(lst.groups) == 2
    assert set(lst.groups.keys()) == {"kernel.sched_latency_ns",
                                      "kernel.timer_migration"}
    for name, g in lst.groups.items():
        assert len(g.functions) == 1
        if name == "kernel.sched_latency_ns":
            assert list(g.functions.keys()) == ["sched_proc_update_handler"]
            f = g.functions["sched_proc_update_handler"]
            assert f.mod.llvm == \
                "snapshots-sysctl/linux-3.10.0-957.el7/kernel/sched/fair.ll"
        elif name == "kernel.timer_migration":
            assert list(g.functions.keys()) == ["proc_dointvec_minmax"]
            f = g.functions["proc_dointvec_minmax"]
            assert f.mod.llvm == \
                "snapshots-sysctl/linux-3.10.0-957.el7/kernel/sysctl.ll"
        assert f.tag == "proc handler"
        assert f.glob_var is None
