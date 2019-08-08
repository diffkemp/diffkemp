# *** Script for debugging return values for methods in FunctionComparator ***
# *** Usage: gdb --batch --command=tools/func_comparator_return_vals.gdb   ***
# ***        --args <simpll_call>                                          ***
# *** Note: It helps to run this script using the actual function where    ***
# ***       the problem is instead of the KABI symbol, because there       ***
# ***       will be less output.                                           ***

# First set up the class.
python
class PrintReturnValueBreakpoint (gdb.FinishBreakpoint):
    def __init__(self, left_val, right_val, function):
        # Breakpoint declared as internal to avoid unnecessary output which
        # would otherwise slow down execution.
        super().__init__(internal=True)

        # left_val and right_val are pointers to values that are compared.
        # The stop method is called at the point between the function return
        # and the assignment of the return value (i.e. the stack for the
        # function call has already been popped), therefore it is necessary to
        # pass these values at the time of function entry. (the implementation
        # in LLVM makes sure the values will persist).
        self.left_val = left_val
        self.right_val = right_val

        # function is the name of the function.
        # This can't be detected from the frame due to the same reasons as
        # described above.
        self.function = function
    def stop(self):
        # Print only functions that ended with a value different than zero.
        # This is to avoid priting too much output.
        if gdb.parse_and_eval("$eax") != 0:
            # First print the return value.
            print("Function %s returned %s." % (self.function,
                  gdb.parse_and_eval("$eax")))

            # Dump the values.
            command_l = "p valueToString(%s)" % str(self.left_val)
            command_r = "p valueToString(%s)" % str(self.right_val)
            raw_dump_l = gdb.execute(command_l, to_string=True).rstrip()
            raw_dump_r = gdb.execute(command_r, to_string=True).rstrip()

            # Parse the raw output.
            # Note: The raw output looks like $<number> = "<dump>".
            value_l = raw_dump_l[raw_dump_l.find('"') + 1:raw_dump_l.rfind('"')]
            value_r = raw_dump_r[raw_dump_r.find('"') + 1:raw_dump_r.rfind('"')]

            # Print the values.
            print("\tL: %s" % value_l)
            print("\tR: %s" % value_r)
        # Do not pause execution.
        return False
end

# Then set up the breakpoints.
break FunctionComparator::cmpValues
break FunctionComparator::cmpOperations

# On the breakpoint, set up the finish breakpoint defined above.
commands 1
  silent
  py PrintReturnValueBreakpoint(gdb.parse_and_eval("L"), \
                                gdb.parse_and_eval("R"), "cmpValues")
  continue
end

commands 2
  silent
  py PrintReturnValueBreakpoint(gdb.parse_and_eval("L"), \
                                gdb.parse_and_eval("R"), "cmpOperations")
  continue
end

# At the end, run the program.
run

# After the program ends, quit.
quit
