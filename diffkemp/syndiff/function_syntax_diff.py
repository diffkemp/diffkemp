"""
Syntax difference of two functions - using diff utility and filtering the
result.
"""
from __future__ import absolute_import
from subprocess import check_output, CalledProcessError


def _is_alpha_or_underscore(char):
    return char.isalpha() or char == "_"


def _is_function_header(diff_line):
    # Line is a function header if it starts with a letter or underscore and
    # does not end with ':' (then its a label)
    if len(diff_line) > 2:
        return (_is_alpha_or_underscore(diff_line[2]) and
                not diff_line.endswith(":"))
    return False


def _is_header_for_function(header, fun):
    # Line is a header for a function fun if it contains fun which is not
    # preceded of followed by a letter or by an underscore (then it's a header
    # for a different function)
    pos = header.find(fun)
    return (pos > 0 and not _is_alpha_or_underscore(header[pos - 1]) and
            not (len(header) > pos + len(fun) and
                 _is_alpha_or_underscore(header[pos + len(fun)])))


def _is_comments_only_chunk(chunk):
    # Chunk contains only comments if all changed lines start with:
    #   '/*': start multiline comment in the source
    #   '*' :  continuation multiline comment in the source
    #   '//': line comment in the source
    # Note that in a chunk, the diff result is indented by 4 spaces and the
    # original source code is moreover indented by 2 characters (the first of
    # them can be '+', '-', or '!').
    for line in chunk.splitlines():
        # Ignore empty lines
        if line.isspace():
            continue
        # Ignore diff-specific lines
        if line.startswith("***") or line.startswith("---"):
            continue
        # Ignore unchanged lines
        if not (line[0] == "+" or line[0] == "-" or line[0] == "!"):
            continue
        # If there is a non-comment line, return False
        if not (line[2:].lstrip().startswith("/*") or
                line[2:].lstrip().startswith("*") or
                line[2:].lstrip().startswith("//")):
            return False
    return True


def _add_chunk_to_output(chunk, output):
    # Chunk is added to the output if it contains something else than comments
    if not _is_comments_only_chunk(chunk):
        output += chunk
    return output


def syntax_diff(first_file, second_file, fun):
    """Get diff of a C function fun between first_file and second_file"""
    # Run diff with showing lines with function headers for each chunk
    # However, the function headers may be incorrect in some cases, so we need
    # some further processing below.
    command = ["/diffutils/src/diff", first_file, second_file, "-C", "1", "-F",
               br"^[[:alpha:]\$_][^:]*$"]
    try:
        diff = check_output(command)
    except CalledProcessError as e:
        if e.returncode == 1:
            diff = e.output
        else:
            raise

    # Filter only those lines that are in a diff chunk of the given function
    output_diff = ""
    function_chunk = ""
    in_fun = False
    lines_num = ""
    for line in diff.splitlines():
        # Store the line with lines numbers of the chunk
        if line.startswith("*** "):
            # Old file lines
            lines_num = line
        if line.startswith("---"):
            # New file lines - if we are in a function chunk, the line will
            # be included, no need to store it.
            if not in_fun:
                lines_num = line
            else:
                lines_num = ""

        # New chunk begins
        if line.startswith("***************"):
            # Print the previous chunk if it corresponds to the given function
            if in_fun:
                output_diff = _add_chunk_to_output(function_chunk, output_diff)
            function_chunk = ""
            # Check if the new context function is the correct one
            in_fun = _is_header_for_function(line, fun)

        # Chunk contains a function header - the given context is incorrect
        # We are truly in a chunk given by the header.
        if _is_function_header(line):
            # If the last chunk that was found this way (does not start
            # with the context prefix), add it to the output.
            if in_fun and not function_chunk.startswith("****"):
                output_diff = _add_chunk_to_output(function_chunk, output_diff)
            # New chunk does not contain the line number: initialize it
            function_chunk = "{}\n".format(lines_num) if lines_num else ""
            # Check if the new context function is the correct one
            in_fun = _is_header_for_function(line, fun)

        # If inside a chunk for of compared function, store the line
        if in_fun:
            function_chunk += "{}\n".format(line)

    # Add the last chunk to the output if necessary
    if in_fun:
        output_diff = _add_chunk_to_output(function_chunk, output_diff)

    return output_diff
