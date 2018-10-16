from __future__ import absolute_import

from subprocess import check_output, CalledProcessError


def syntax_diff(first_file, second_file, fun):
    """Get diff of a C function fun between first_file and second_file"""
    # Run diff with showing lines with function headers for each chunk
    command = ["diff", first_file, second_file, "-C", "1", "-F",
               "^[[:alpha:]\$_][^:]*$"]
    try:
        diff = check_output(command)
    except CalledProcessError as e:
        if e.returncode == 1:
            diff = e.output
        else:
            raise

    # Filter only those lines that are preceded by a line with the given
    # function name
    filter_diff = ""
    in_fun = False
    for line in diff.splitlines():
        if line.startswith("***************"):
            in_fun = fun in line
        if in_fun:
            filter_diff += "    {}\n".format(line)
    return filter_diff
