# configuration.py
"""Provides functions to read/write simtemp driver sysfs configuration."""

import typing
from config_file import (
    SAMPLING_MS_PATH, THRESHOLD_MC_PATH, MODE_PATH, STATS_PATH
)

def set_config_value(path: str, value: str) -> bool:
    """Writes a string value to a specified sysfs file.

    Requires appropriate permissions (e.g., run as root or use sudo tee).

    Args:
        path: The absolute path to the sysfs attribute file.
        value: The string value to write.

    Returns:
        True if the write was successful, False otherwise.
    """
    try:
        # Using 'w' truncates the file before writing, which is standard for sysfs
        with open(path, 'w') as f:
            f.write(value + '\n') # Sysfs usually expects a newline
        return True
    except (FileNotFoundError, PermissionError, OSError) as e:
        print(f"Error writing to {path}: {e}")
        return False

def get_config_value(path: str) -> typing.Optional[str]:
    """Reads a string value from a specified sysfs file.

    Args:
        path: The absolute path to the sysfs attribute file.

    Returns:
        The string value read (stripped of whitespace) on success, None otherwise.
    """
    try:
        with open(path, 'r') as f:
            value = f.read().strip()
        return value
    except (FileNotFoundError, PermissionError, OSError) as e:
        print(f"Error reading from {path}: {e}")
        return None

def set_sampling_ms(period: int) -> bool:
    """Sets the sampling period in milliseconds.

    Args:
        period: The desired sampling period (integer).

    Returns:
        True on success, False on failure.
    """
    print(f"Setting sampling_ms to {period}...")
    return set_config_value(SAMPLING_MS_PATH, str(period))

def get_sampling_ms() -> typing.Optional[int]:
    """Gets the current sampling period in milliseconds.

    Returns:
        The sampling period as an integer, or None on error.
    """
    value_str = get_config_value(SAMPLING_MS_PATH)
    if value_str is not None:
        try:
            return int(value_str)
        except ValueError:
            print(f"Error: Could not parse sampling_ms value '{value_str}' as integer.")
            return None
    return None

def set_threshold_mc(threshold: int) -> bool:
    """Sets the alert threshold in milli-degrees Celsius.

    Args:
        threshold: The desired threshold (integer).

    Returns:
        True on success, False on failure.
    """
    print(f"Setting threshold_mc to {threshold}...")
    return set_config_value(THRESHOLD_MC_PATH, str(threshold))

def get_threshold_mc() -> typing.Optional[int]:
    """Gets the current alert threshold in milli-degrees Celsius.

    Returns:
        The threshold as an integer, or None on error.
    """
    value_str = get_config_value(THRESHOLD_MC_PATH)
    if value_str is not None:
        try:
            return int(value_str)
        except ValueError:
            print(f"Error: Could not parse threshold_mc value '{value_str}' as integer.")
            return None
    return None

def set_mode(mode: str) -> bool:
    """Sets the simulation mode.

    Args:
        mode: The desired mode string (e.g., "normal", "noisy", "ramp").

    Returns:
        True on success, False on failure.
    """
    print(f"Setting mode to '{mode}'...")
    # Basic validation, driver handles definitive validation
    valid_modes = ["normal", "noisy", "ramp"]
    if mode not in valid_modes:
        print(f"Warning: Mode '{mode}' may not be valid. Allowed: {valid_modes}")
    return set_config_value(MODE_PATH, mode)

def get_mode() -> typing.Optional[str]:
    """Gets the current simulation mode.

    Returns:
        The mode string, or None on error.
    """
    return get_config_value(MODE_PATH)

def get_stats() -> typing.Optional[str]:
    """Gets the driver statistics string.

    Returns:
        The raw statistics string (e.g., "updates=X alerts=Y errors=Z"),
        or None on error.
    """
    return get_config_value(STATS_PATH)