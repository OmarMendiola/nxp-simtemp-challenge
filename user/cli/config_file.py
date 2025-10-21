# config_file.py
"""Stores configuration constants and paths for the simtemp driver CLI."""

import os # Used to build paths dynamically
import zoneinfo #python3.9+ for timezone handling

# Base paths (adjust if your misc device name or class path differs)
# Note: The actual device name might be dynamic (e.g., /dev/simtemp-nxp_simtemp.0)
#       Adjust DRIVER_DEV_PATH or use glob in the main app if needed.
DRIVER_BASE_NAME = "simtemp" # Assumes the simple name or a symlink
DRIVER_DEV_PATH = f"/dev/{DRIVER_BASE_NAME}"
DRIVER_SYSFS_PATH = f"/sys/class/misc/{DRIVER_BASE_NAME}"

# Sysfs attribute paths
SAMPLING_MS_PATH = os.path.join(DRIVER_SYSFS_PATH, "sampling_ms")
THRESHOLD_MC_PATH = os.path.join(DRIVER_SYSFS_PATH, "threshold_mc")
MODE_PATH = os.path.join(DRIVER_SYSFS_PATH, "mode")
STATS_PATH = os.path.join(DRIVER_SYSFS_PATH, "stats")

# Test result codes
TEST_FAIL_CODE: int = -1
TEST_PASS_CODE: int = 0

# Sample structure format (Little-endian: <, u64: Q, s32: i, u32: I)
# Corresponds to: __u64 timestamp_ns; __s32 temp_mc; __u32 flags;
SAMPLE_FORMAT: str = "<QiI"
SAMPLE_SIZE_BYTES: int = 16 # 8 + 4 + 4

# Driver flags (mirroring kernel/nxp_simtemp.h)
SIMTEMP_SAMPLE_FLAG_NEW: int = (1 << 0)
SIMTEMP_SAMPLE_FLAG_THRESHOLD_HI: int = (1 << 1)
# Add other flags here if defined in the driver

# Timezone for displaying timestamps (UTC-6)
DISPLAY_TIMEZONE = zoneinfo.ZoneInfo("America/Mexico_City")