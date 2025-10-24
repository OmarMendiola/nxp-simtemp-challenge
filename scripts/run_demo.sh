#!/usr/bin/env bash
#
# run_demo.sh: Loads, configures, tests, and unloads the kernel module.
#
# Executes a full sequence to test the functionality:
# 1. Loads the module (.ko) into the kernel.
# 2. Configures the device via sysfs.
# 3. Runs the user-space CLI application to interact with the module.
# 4. Unloads the module from the kernel, ensuring cleanup.

# --- Bash Interpreter Check ---
# This script uses bash-specific features. Exit if not run with bash.
if [ -z "${BASH_VERSION}" ]; then
    echo "ERROR: This script requires bash. Please run it using 'bash scripts/run_demo.sh' or by making it executable ('chmod +x scripts/run_demo.sh') and running './scripts/run_demo.sh'." >&2
    exit 1
fi

# --- Configuration ---
# Fail the script immediately if a command fails.
set -e
# Treat unset variables as an error.
set -u

# --- Path Setup ---
# Get the directory where this script is located to run from anywhere.
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
# Assume the project root is one directory above the script directory.
PROJECT_ROOT=$(dirname "${SCRIPT_DIR}")

# --- Variables ---
MODULE_NAME="nxp_simtemp"
MODULE_FILE="${PROJECT_ROOT}/kernel/${MODULE_NAME}.ko"
USER_APP_CLI="${PROJECT_ROOT}/user/cli/main.py"
# Device entries created by the driver
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

if [ ! -f "${USER_APP_CLI}" ]; then
    echo "ERROR: User CLI application '${USER_APP_CLI}' not found."
    exit 1
fi

# --- Automatic Cleanup ---
# Defines a cleanup function that will be executed when the script exits,
# whether on success, error, or interruption (Ctrl+C).
cleanup() {
  echo "INFO: Running cleanup..."
  # 'lsmod' checks if the module is loaded, and 'grep -q' suppresses output.
  if lsmod | grep -q "^${MODULE_NAME}"; then
    echo "INFO: Unloading module '${MODULE_NAME}'..."
    # The '|| true' part ensures that the script doesn't exit with an error
    # if rmmod fails (e.g., if the device is busy).
    rmmod "${MODULE_NAME}" || echo "WARN: rmmod failed, but continuing cleanup."
  fi
  echo "INFO: Cleanup complete."
}

# 'trap' registers the 'cleanup' command to run on the EXIT signal.
trap cleanup EXIT

# --- Demo Execution ---
echo "INFO: Loading module '${MODULE_NAME}'..."
insmod "${MODULE_FILE}"

echo "INFO: Verifying that the device has been created..."
# Wait up to 2 seconds for the device node and sysfs to appear
for i in {1..10}; do
    if [ -e "${DEV_ENTRY}" ] && [ -d "${SYSFS_PATH}" ]; then
        break
    fi
    sleep 0.2
done

if [ ! -e "${DEV_ENTRY}" ]; then
    echo "ERROR: Module loaded, but device entry '${DEV_ENTRY}' does not exist."
    # Cleanup will run automatically due to 'trap'
    exit 1
fi
if [ ! -d "${SYSFS_PATH}" ]; then
    echo "ERROR: Module loaded, but sysfs entry '${SYSFS_PATH}' does not exist."
    exit 1
fi
echo "INFO: Device found at ${DEV_ENTRY} and sysfs at ${SYSFS_PATH}."

echo "INFO: Configuring module via sysfs (setting mode to 'noisy')..."
echo "noisy" > "${SYSFS_PATH}/mode"

echo "INFO: Reading back configuration to verify..."
CURRENT_MODE=$(cat "${SYSFS_PATH}/mode")
echo "INFO: Current mode is now '${CURRENT_MODE}'."
if [ "${CURRENT_MODE}" != "noisy" ]; then
    echo "ERROR: Failed to set mode!"
    exit 1
fi

echo "INFO: Launching user-space CLI self-test..."
echo "      (The test will run automatically. It takes about 10 seconds)."
echo "----------------------------------------------------"
# The python script's main function runs a menu. We pipe the commands to it:
# '3' to run Start Periodic Sampling (Ctrl+C to stop), and '5' to exit the CLI.
printf "3\n5\n" | python3 "${USER_APP_CLI}"
echo "----------------------------------------------------"
echo "INFO: User-space test finished."

# Cleanup will run automatically on exit.
echo "INFO: Demo completed successfully."

