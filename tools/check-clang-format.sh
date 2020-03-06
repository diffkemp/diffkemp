#!/bin/bash
# Author: Petr Silling, psilling@redhat.com

NAME=$(basename "$0")
START_PATH="."
CONTEXT=""
INPLACE=false
STYLE="file"
VERSION=""
FAIL_COUNT=0
FAIL_FILENAMES=""
FIND_EXCLUSIONS=()
FIND_OPTIONS=( -type f \( -iname '*.h' -o -iname '*.cpp' \) )
PRINT_NEW_LINE=false
DIFFKEMP_CHECK=false

# Print usage and exit.
usage() {
    printf "%s\n\n%s\n\n%s\n%-25s%s\n\n%s\n%-25s%s\n%-25s%s\n%-25s%s\n%-25s%s\n\
%-25s%s\n%-25s%s\n%-25s%s\n%-25s%s\n" \
    "Diff format checker for C++ files." \
    "Usage: ${NAME} [options] [start-path]" \
    "Arguments:" \
    "  start-path" "directory path to recursively search for files from" \
    "Options:" \
    "  -c" "use the context option with diff" \
    "  -d" "run with the default Diffkemp C++ directory settings" \
    "  -e <path>" "exclude a directory given by <path>" \
    "  -h" "show this message and exit" \
    "  -i" "fix the format using the clang-format inplace option instead" \
    "  -s <style>" "override the default 'file' clang-format style" \
    "  -q" "supress all output" \
    "  -v <version>" "use a specific clang-format <version>"
    exit 0
}

# Process usage options.
while getopts ":cde:his:qv:" opt; do
    case "${opt}" in
        c)
            CONTEXT="-c"
            ;;
        d)
            DIFFKEMP_CHECK=true
            ;;
        e)
            if [ -z "${OPTARG}" ]; then
                echo "ERROR: No exclusion path has been specified." >&2
                usage
            fi
            FIND_EXCLUSIONS+=( -not -path "*${OPTARG}/*" )
            ;;
        i)
            INPLACE=true
            ;;
        s)
            STYLE=$(echo "${OPTARG}" | awk '{ print tolower($0) }')
            if [ "${STYLE}" != "file" ] && [ "${STYLE}" != "llvm" ] \
                && [ "${STYLE}" != "google" ] && [ "${STYLE}" != "chromium" ] \
                && [ "${STYLE}" != "mozilla" ] && [ "${STYLE}" != "webkit" ] \
                && [ "${STYLE}" != "microsoft"  ]; then
                echo "ERROR: Invalid clang-format style supplied." >&2
                usage
            fi
            ;;
        q)
            exec >/dev/null 2>&1
            ;;
        v)
            VERSION="${OPTARG}"
            if [[ ! "${VERSION}" =~ ^[0-9.]+$ ]]; then
                echo "ERROR: Invalid clang-format version supplied." >&2
                usage
            fi
            VERSION=-"${VERSION}"
            ;;
        *)
            usage
            ;;
    esac
done

shift $((OPTIND - 1))

# Set optional start path if given.
if [ ! -z "$1" ]; then
    START_PATH="$1"
fi

# Try to set the default Diffkemp find options based on current directory.
if [ "${DIFFKEMP_CHECK}" = true ]; then
    git_root=$(git rev-parse --show-toplevel 2> /dev/null)
    if [ "$?" -ne 0  ]; then
        echo "ERROR: Cannot find the Diffkemp root directory." >&2
        exit 1
    fi

    FIND_EXCLUSIONS=( -not -path "*llvm-lib/*" )
    if [ "${git_root}" = "${PWD}" ]; then
        START_PATH="diffkemp/"
        TESTS_START_PATH="tests/unit_tests/simpll/"
    else
        START_PATH="${git_root}/diffkemp/"
        TESTS_START_PATH="${git_root}/tests/unit_tests/simpll/"
    fi
fi

# Get filenames to process.
FILENAMES=$(find "${START_PATH}" "${TESTS_START_PATH}" \
"${FIND_EXCLUSIONS[@]}" "${FIND_OPTIONS[@]}" 2> /dev/null)

# Check commands and directory files.
if [ "$?" -ne 0 ]; then
    echo "ERROR: Directory not found." >&2
    exit 1
elif ! command -v clang-format"${VERSION}" >/dev/null 2>&1; then
    echo "ERROR: Could not find clang-format${VERSION}." >&2
    exit 1
elif [ -z "$FILENAMES" ]; then
    echo "No files to check."
    exit 0
fi

# Loop ever all *.cpp and *.h files in given path subdirectories and check their
# formatting using clang-format and diff. A non-empty diff signals an error.
while read cfile; do
    if [ "${INPLACE}" = true ]; then
        echo "Formatting file ${cfile}"
        clang-format"${VERSION}" -i -style="${STYLE}" "${cfile}"
    else
        echo "Checking the format of file ${cfile}"
        diff ${CONTEXT} "${cfile}" \
            <(clang-format"${VERSION}" -style="${STYLE}" "${cfile}")
        exit_code="$?"

        if [ "${exit_code}" -eq 1 ]; then
            printf "\n"
            PRINT_NEW_LINE=false
            (( FAIL_COUNT++ ))
            FAIL_FILENAMES=$(printf "%s\n%s" "${FAIL_FILENAMES}" "${cfile}")
        else
            PRINT_NEW_LINE=true
        fi
    fi
done <<< "${FILENAMES}"

if [ "${INPLACE}" = true ]; then
    printf "\n%s\n" "All C++ files have been formatted."
    exit 0
elif [ "${PRINT_NEW_LINE}" = true ]; then
    printf "\n"
fi

# Print information about the check as a whole.
if [ "${FAIL_COUNT}" -gt 0 ]; then
    printf "%s%s\n%s\n"\
           "Found ${FAIL_COUNT} incorrectly formatted C++ files:"\
           "${FAIL_FILENAMES}"\
           "See the diff output above this log."
    exit 1
else
    echo "All C++ files are formatted correctly."
fi

exit 0
