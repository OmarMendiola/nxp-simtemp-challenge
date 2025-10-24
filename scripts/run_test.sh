#!/usr/bin/env bash
#
# run_test.sh: Loads, verifies, tests, and unloads the kernel module.
#
# Incorporates TS1 tests and then runs the automated Python test suite (TP1-TP6).

# --- Bash Interpreter Check ---
# This script uses bash-specific features. Exit if not run with bash.
if [ -z "${BASH_VERSION}" ]; then
    echo "ERROR: This script requires bash. Please run it using 'bash scripts/run_test.sh' or by making it executable ('chmod +x scripts/run_test.sh') and running './scripts/run_test.sh'." >&2
    exit 1
fi

# --- Configuration ---
set -e # Exit immediately if a command exits with a non-zero status.
set -u # Treat unset variables as an error.

# --- Path Setup ---
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
PROJECT_ROOT=$(dirname "${SCRIPT_DIR}")

# --- Variables ---
MODULE_NAME="nxp_simtemp"
MODULE_FILE="${PROJECT_ROOT}/kernel/${MODULE_NAME}.ko"
USER_TEST_SCRIPT="${PROJECT_ROOT}/user/cli/test_mode.py"
DEV_ENTRY="/dev/simtemp"
SYSFS_PATH="/sys/class/misc/simtemp"

# --- Pre-flight Checks ---
if [ "$(id -u)" -ne 0 ]; then
  echo "ERROR: This script must be run with root privileges (sudo)." >&2
  exit 1
fi

if [ ! -f "${MODULE_FILE}" ]; then
    echo "ERROR: Module file '${MODULE_FILE}' not found."
    echo "Please build the project first with 'scripts/build.sh'."
    exit 1
fi

if [ ! -f "${USER_TEST_SCRIPT}" ]; then
    echo "ERROR: User test script '${USER_TEST_SCRIPT}' not found."
    exit 1
fi

# Check for python3
if ! command -v python3 &> /dev/null; then
    echo "ERROR: python3 command not found. Please install Python 3."
    exit 1
fi

# --- Automatic Cleanup ---
cleanup() {
  local exit_status=$? # Capture the exit status of the last command before cleanup
  echo "INFO: Running cleanup (Script exited with status ${exit_status})..."
  if lsmod | grep -q "^${MODULE_NAME}"; then
    echo "INFO: Unloading module '${MODULE_NAME}'..."
    # Check dmesg before unload
    dmesg -c > /dev/null # Clear buffer before checking unload messages
    if ! rmmod "${MODULE_NAME}"; then
        echo "ERROR: Failed to unload module '${MODULE_NAME}'. Check if it's in use."
        # Don't overwrite the original exit status if rmmod failed
        exit_status=1
    else
        # Check dmesg for errors during unload
        if dmesg | grep -iE 'warn|error|fail|trace|bug'; then
            echo "WARN: Potential issues detected in dmesg during module unload."
            # Optionally set exit_status=1 here if unload warnings are critical
        else
            echo "INFO: Module unloaded cleanly."
        fi
    fi
  else
    echo "INFO: Module '${MODULE_NAME}' was not loaded or already unloaded."
  fi
  echo "INFO: Cleanup complete."
  # Exit with the original status unless cleanup specifically failed critically
  exit ${exit_status}
}
trap cleanup EXIT INT TERM # Trap exit, Ctrl+C, and termination signals

# --- TS1: Load Module and Verify ---
echo "--- Starting TS1: Module Load Verification ---"
echo "INFO: Clearing dmesg buffer..."
dmesg -C || echo "WARN: Failed to clear dmesg buffer (might lack permissions)."

# Ensure module is not loaded initially
if lsmod | grep -q "^${MODULE_NAME}"; then
    echo "INFO: Module ${MODULE_NAME} already loaded. Attempting to unload first..."
    rmmod "${MODULE_NAME}" || { echo "ERROR: Failed to unload pre-existing module. Aborting."; exit 1; }
    sleep 1 # Give it a second
fi

echo "INFO: Loading module '${MODULE_FILE}'..."
insmod "${MODULE_FILE}"

echo "INFO: Checking dmesg for load errors/warnings..."
# Check dmesg output for common error indicators immediately after loading
# The '-E' enables extended regex, '-i' is case-insensitive
if dmesg | grep -iE 'warn|error|fail|trace|bug|taint'; then
    echo "ERROR: Potential issues detected in dmesg during module load. Check dmesg log."
    # Optionally: exit 1 here if any dmesg output is unacceptable
else
    echo "INFO: No immediate errors/warnings found in dmesg after load."
fi

echo "INFO: Verifying device node and sysfs entries..."
# Wait up to 2 seconds for the device node and sysfs to appear
for i in {1..10}; do
    if [ -e "${DEV_ENTRY}" ] && [ -d "${SYSFS_PATH}" ]; then
        break
    fi
    echo "INFO: Waiting for device creation..."
    sleep 0.2
done

if [ ! -e "${DEV_ENTRY}" ]; then
    echo "ERROR: Device entry '${DEV_ENTRY}' did not appear after loading module."
    exit 1 # Cleanup will run via trap
fi
if [ ! -d "${SYSFS_PATH}" ]; then
    echo "ERROR: Sysfs entry '${SYSFS_PATH}' did not appear after loading module."
    exit 1 # Cleanup will run via trap
fi
echo "INFO: Device nodes and sysfs entries created successfully."
echo "--- TS1: Module Load Verification PASSED ---"
echo ""

# --- TS2: Run Python Functional Tests ---
echo "--- Starting TS2: Python Functional Tests (TP1-TP6) ---"
echo "INFO: Executing '${USER_TEST_SCRIPT}'..."
echo "----------------------------------------------------"

# Run the python script directly. It will print its own pass/fail status.
# The script will exit with 0 on overall success, non-zero on failure.
if ! python3 "${USER_TEST_SCRIPT}"; then
    echo "----------------------------------------------------"
    echo "ERROR: Python test suite (TS2) reported failure."
    exit 1 # Cleanup will run via trap
fi

echo "----------------------------------------------------"
echo "--- TS2: Python Functional Tests PASSED ---"
echo ""

# --- Success ---
# If we reach here, all tests passed. The cleanup will run on normal exit.
echo "INFO: All tests completed successfully."
exit 0