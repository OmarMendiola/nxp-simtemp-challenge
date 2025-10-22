#!/bin/bash
#
# build.sh: Builds the kernel module.
#
# This script automatically detects the correct path for kernel headers,
# supporting both standard Linux distributions and the WSL2 environment.
# It fails with helpful hints if headers are not found.

# --- Configuration ---
# Fail the script immediately if a command fails.
set -e
# Treat unset variables as an error.
set -u

# --- Path Setup ---
# Get the directory where this script is located, so it can be run from anywhere.
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
# Assume the project root is one directory above the script directory.
PROJECT_ROOT=$(dirname "${SCRIPT_DIR}")
KERNEL_MODULE_DIR="${PROJECT_ROOT}/kernel"

# --- Kernel Source/Header Detection ---
KERNEL_VERSION=$(uname -r)
# Default path for kernel headers on most systems
KERNEL_DIR="/lib/modules/${KERNEL_VERSION}/build"

# Special handling for WSL2, which requires manually cloned kernel source
if [[ "${KERNEL_VERSION}" == *"WSL2"* ]]; then
    echo "INFO: WSL2 kernel detected."
    WSL2_KERNEL_SOURCE_DIR="${HOME}/WSL2-Linux-Kernel"

    if [ -d "${WSL2_KERNEL_SOURCE_DIR}" ]; then
        echo "INFO: Found manually cloned WSL2 kernel source at ${WSL2_KERNEL_SOURCE_DIR}."
        # Override the default KERNEL_DIR to point to the WSL2 source
        KERNEL_DIR="${WSL2_KERNEL_SOURCE_DIR}"
    else
        # If the WSL2 source isn't found, provide specific instructions to the user.
        echo "ERROR: WSL2 kernel source not found at '${WSL2_KERNEL_SOURCE_DIR}'."
        echo "You must clone the source from GitHub and prepare it for building modules."
        echo ""
        echo "Please run the following commands:"
        KERNEL_BASE_VERSION=$(echo "${KERNEL_VERSION}" | cut -d'-' -f1)
        EXPECTED_TAG="linux-msft-wsl-${KERNEL_BASE_VERSION}"
        echo "  1. sudo apt-get update && sudo apt-get install -y build-essential flex bison libssl-dev libelf-dev bc"
        echo "  2. git clone https://github.com/microsoft/WSL2-Linux-Kernel.git ${WSL2_KERNEL_SOURCE_DIR}"
        echo "  3. cd ${WSL2_KERNEL_SOURCE_DIR}"
        echo "  4. git checkout ${EXPECTED_TAG}"
        echo "  5. zcat /proc/config.gz > .config"
        echo "  6. make -j\$(nproc) modules_prepare"
        echo ""
        echo "After completing these steps, please re-run this build script."
        exit 1
    fi
fi

# --- Final Verification ---
# After potentially overriding for WSL2, check if the final KERNEL_DIR is valid.
if [ ! -d "${KERNEL_DIR}" ]; then
    echo "ERROR: Kernel headers directory not found at '${KERNEL_DIR}'."
    echo "Please ensure the headers for your current kernel (${KERNEL_VERSION}) are installed."
    echo ""
    echo "On Debian/Ubuntu-based systems, you can usually install them by running:"
    echo "  sudo apt-get update"
    echo "  sudo apt-get install linux-headers-${KERNEL_VERSION}"
    exit 1
fi

echo "INFO: Using kernel source/headers from: ${KERNEL_DIR}"


# --- Kernel Module Compilation ---
MODULE_NAME="nxp_simtemp"
echo "INFO: Building kernel module '${MODULE_NAME}.ko'..."
# -C: Change to the kernel source directory.
# M=: Specifies the location of our kernel module's source and Makefile.
make -C "${KERNEL_DIR}" M="${KERNEL_MODULE_DIR}" modules

echo "INFO: Kernel module built successfully."
echo ""
echo "Build complete. The user-space application is a Python script and does not need compilation."
echo "You can now run the demo with 'scripts/run_demo.sh'."

