# *** Script for debugging return values for methods in FunctionComparator ***
# *** Usage: gdb --batch --command=tools/func_comparator_return_vals.gdb   ***
# ***        --args <simpll_call>                                          ***
# *** Note: It helps to run this script using the actual function where    ***
# ***       the problem is instead of the KABI symbol, because there       ***
# ***       will be less output.                                           ***

# First set up the class.
python
def parse_raw_dump(input):
    return input[input.find('"') + 1:input.rfind('"')]
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
            retval = gdb.parse_and_eval("$eax")

            # Dump the values.
            command_l = "p valueToString(%s)" % str(self.left_val)
            command_r = "p valueToString(%s)" % str(self.right_val)
            raw_dump_l = gdb.execute(command_l, to_string=True).rstrip()
            raw_dump_r = gdb.execute(command_r, to_string=True).rstrip()

            # Get the indentation.
            indent_cmd = "p getDebugIndent(' ')"
            indent_raw = gdb.execute(indent_cmd, to_string=True).rstrip()
            indent = parse_raw_dump(indent_raw)

            # Then print the return value.
            print(indent, end='')
            print("Function %s returned %s." % (self.function, retval))

            # Parse the raw output.
            # Note: The raw output looks like $<number> = "<dump>".
            value_l = parse_raw_dump(raw_dump_l)
            value_r = parse_raw_dump(raw_dump_r)

            # Print the values.
            print(indent, end='')
            print("\tL: %s" % value_l)
            print(indent, end='')
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
