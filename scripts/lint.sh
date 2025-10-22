#!/bin/bash
#
# lint.sh: Runs static analysis and formatting tools.
#
# This optional script looks for common code quality tools for kernel
# development and Python, and runs them if they are available.

# --- Configuration ---
# Treat unset variables as an error.
set -u
# Keep track of the exit code.
EXIT_CODE=0

# --- Path Setup ---
# Get the directory where this script is located, so it can be run from anywhere.
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
# Assume the project root is one directory above the script directory.
PROJECT_ROOT=$(dirname "${SCRIPT_DIR}")
KERNEL_MODULE_DIR="${PROJECT_ROOT}/kernel"
USER_APP_DIR="${PROJECT_ROOT}/user/cli"

# --- checkpatch.pl (for Kernel Code) ---
# First, find the kernel source/headers directory, same logic as build.sh
KERNEL_VERSION=$(uname -r)
KERNEL_DIR="/lib/modules/${KERNEL_VERSION}/build"
if [[ "${KERNEL_VERSION}" == *"WSL2"* ]] && [ ! -d "${KERNEL_DIR}" ]; then
    KERNEL_DIR="${HOME}/WSL2-Linux-Kernel"
fi

CHECKPATCH_PATH="${KERNEL_DIR}/scripts/checkpatch.pl"

if [ -f "${CHECKPATCH_PATH}" ]; then
    echo "INFO: Running checkpatch.pl on kernel files..."
    # --no-tree: Necessary when running outside of the kernel git tree.
    # --terse:   Prints one line per warning/error.
    # -f:        Check individual files.
    # We find all .c and .h files in the kernel directory to check them.
    find "${KERNEL_MODULE_DIR}" -name "*.[ch]" -print0 | xargs -0 "${CHECKPATCH_PATH}" --no-tree --terse -f || EXIT_CODE=$?
else
    echo "WARN: 'checkpatch.pl' not found. Skipping."
    echo "      (Searched in ${CHECKPATCH_PATH})"
fi

# --- clang-format (for C Code Style) ---
if command -v clang-format &> /dev/null; then
    echo "INFO: Checking C code formatting with clang-format..."
    # --dry-run: Show changes without applying them.
    # -Werror: Return a non-zero exit code if changes are needed.
    # We check all .c and .h files in the kernel directory.
    C_FILES=$(find "${KERNEL_MODULE_DIR}" -name "*.[ch]")
    if ! clang-format --dry-run -Werror ${C_FILES}; then
        echo "ERROR: C code does not follow the style guidelines."
        echo "       Run 'clang-format -i ${C_FILES}' to fix."
        EXIT_CODE=1
    else
        echo "INFO: C code formatting is correct."
    fi
else
    echo "WARN: 'clang-format' not found in PATH. Skipping."
fi

# --- flake8 (for Python Code Style) ---
if command -v flake8 &> /dev/null; then
    echo "INFO: Checking Python code style with flake8..."
    if ! flake8 "${USER_APP_DIR}"; then
        echo "ERROR: Python code does not follow PEP8 style guidelines."
        echo "       Please review the flake8 output above to fix the issues."
        EXIT_CODE=1
    else
        echo "INFO: Python code style is correct."
    fi
else
    echo "WARN: 'flake8' not found in PATH. Skipping."
    echo "      (You can install it with 'pip install flake8')"
fi


# --- Final Result ---
echo ""
if [ ${EXIT_CODE} -eq 0 ]; then
    echo "INFO: All lint checks passed."
else
    echo "ERROR: One or more lint checks failed."
fi

exit ${EXIT_CODE}

