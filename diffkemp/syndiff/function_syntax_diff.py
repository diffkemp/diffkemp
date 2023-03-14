"""
Syntax difference of two functions - using diff utility and filtering the
result.
"""

from subprocess import check_output, CalledProcessError
from tempfile import mkdtemp
from diffkemp.utils import get_end_line, EndLineNotFound

import os
from enum import IntEnum
import re

DIFF_NOT_OBTAINED_MESSAGE = "  [could not obtain diff]\n"
UNIFIED_HUNK_HEAD_REGEX = re.compile(r"""^@@\ -(\d+) # from file start
                                         ((?:,\d+)?) # from file count
                                         \           # space
                                         \+(\d+)     # to file start
                                         ((?:,\d+)?) # to file count
                                         \ @@$""", re.VERBOSE)


def syntax_diff(first_file, second_file, name, kind, first_line, second_line):
    """Get diff of a C function or type between first_file and second_file"""
    try:
        first_end = get_end_line(first_file, first_line, kind)
        second_end = get_end_line(second_file, second_line, kind)
        diff, first_file_fragment, _ = \
            make_diff(diff_format=DiffFormat.CONTEXT,
                      first_file=first_file, second_file=second_file,
                      first_start=first_line, second_start=second_line,
                      first_end=first_end, second_end=second_end)
    except (UnicodeDecodeError, EndLineNotFound):
        return DIFF_NOT_OBTAINED_MESSAGE

    if diff.isspace() or diff == "":
        # Empty diff
        return diff

    # Split off filename names and fix line numbers
    diff_lines = diff.split('\n')[2:]
    diff_lines_new = []

    for line in diff_lines:
        def fix_line(x):
            offset = first_line if polarity == "*" else second_line
            return str(int(x) + offset - 1)

        # Add function header
        if set(list(line)) == set(["*"]):
            with open(os.path.join(first_file_fragment), "r") as extract:
                line += " " + extract.readline().strip()

        # Check whether the line is a line number line
        number_line_set = set([" ", "*", "-", ","] +
                              list(map(str, list(range(0, 10)))))
        if ((not set(list(line)).issubset(number_line_set)) or
            (not any(char.isdigit() for char in line)) or
                line.isspace() or line == ""):
            diff_lines_new += [line]
            continue

        polarity = "*" if line.count("*") > 1 else "-"

        line = line.replace("*", "").replace("-", "").replace(" ", "")
        line = ",".join(map(fix_line, line.split(",")))
        line = polarity * 3 + " " + line + " " + polarity * 3

        diff_lines_new += [line]
    diff = "\n".join(diff_lines_new)

    return diff


class DiffFormat(IntEnum):
    """Enumeration type for possible syntax diff formats."""
    CONTEXT = 0
    UNIFIED = 1


def make_diff(diff_format, first_file, second_file, first_start, second_start,
              first_end, second_end):
    """Creates diff between to fragments of files specified by start and end
    line. Does not fix the line numbers.
    Returns tuple containing:
      - diff,
      - path to file with selected fragment of first file,
      - path to file with selected fragment of second file.
    """
    tmpdir = mkdtemp()

    if diff_format == DiffFormat.CONTEXT:
        option = "-C"
    elif diff_format == DiffFormat.UNIFIED:
        option = "-U"

    first_file_fragment = os.path.join(tmpdir, "1")
    second_file_fragment = os.path.join(tmpdir, "2")

    command = ["diff", option, "1", first_file_fragment, second_file_fragment]

    extract_code(first_file, first_start, first_end, first_file_fragment)
    extract_code(second_file, second_start, second_end, second_file_fragment)

    # check_output fails when the two files are different due to the error code
    # (1), which in fact signalizes success; the exception has to be caught and
    # the error code evaluated manually
    try:
        diff = check_output(command).decode('utf-8')
    except CalledProcessError as e:
        if e.returncode == 1:
            diff = e.output.decode('utf-8')
        else:
            raise
    return diff, first_file_fragment, second_file_fragment


def extract_code(file, start, end, output_file_path):
    """Extracts code (e. g. function) from file from start to end line,
    saves it in the output_file_path"""
    with open(file, "r", encoding='utf-8') as input_file, \
        open(output_file_path, "w",
             encoding='utf-8') as output_file:
        try:
            lines = input_file.readlines()
        except UnicodeDecodeError:
            raise

        for line in lines[start - 1:end]:
            output_file.write(line)


def unified_syntax_diff(first_file, second_file, first_line, second_line,
                        first_end, second_end):
    diff, *_ = make_diff(diff_format=DiffFormat.UNIFIED,
                         first_file=first_file, second_file=second_file,
                         first_start=first_line, second_start=second_line,
                         first_end=first_end, second_end=second_end)

    # Empty diff
    if diff.isspace() or diff == "":
        return diff

    # Fixing line numbers
    diff_lines = diff.split('\n')
    diff_lines_new = []
    for line in diff_lines:
        match = UNIFIED_HUNK_HEAD_REGEX.match(line)
        if match:
            line = f"@@ -{int(match.group(1))+first_line-1}{match.group(2)} " \
                   + f"+{int(match.group(3))+second_line-1}{match.group(4)} @@"
        diff_lines_new += [line]
    diff = "\n".join(diff_lines_new)
    return diff
