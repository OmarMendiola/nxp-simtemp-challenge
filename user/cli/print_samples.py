# print_samples.py
"""Monitors and prints temperature samples from the simtemp driver."""

import select
import os
import struct
from datetime import datetime, timezone
import typing
import fcntl # For setting O_NONBLOCK if needed, though poll handles waiting

from config_file import (
    DRIVER_DEV_PATH, SAMPLE_FORMAT, SAMPLE_SIZE_BYTES,
    SIMTEMP_SAMPLE_FLAG_THRESHOLD_HI, DISPLAY_TIMEZONE
)

def format_timestamp_ns(timestamp_ns: int) -> str:
    """Converts nanoseconds (monotonic) to a more readable UTC-like format.

    Note: Kernel monotonic time doesn't directly map to UTC wall clock time.
          This provides an approximation for display.

    Args:
        timestamp_ns: Timestamp in nanoseconds from the driver.

    Returns:
        Formatted string (YYYY-MM-DDTHH:MM:SS.fffZ).
    """
    try:
        # Convert ns to seconds and create datetime object (relative to epoch)
        dt_object = datetime.utcfromtimestamp(timestamp_ns / 1_000_000_000)
        # Format with milliseconds
        return dt_object.strftime('%Y-%m-%dT%H:%M:%S.') + f"{int((timestamp_ns % 1_000_000_000) / 1_000_000):03d}Z"
    except Exception:
        # Fallback if timestamp is out of range for standard epoch conversion
        return f"{timestamp_ns}ns"

def parse_sample(raw_data: bytes) -> typing.Optional[typing.Tuple[int, int, int]]:
    """Parses the raw binary data from the driver into sample components.

    Args:
        raw_data: The bytes read from the device.

    Returns:
        A tuple (timestamp_ns, temp_mc, flags) or None if parsing fails.
    """
    if len(raw_data) != SAMPLE_SIZE_BYTES:
        print(f"Error: Read {len(raw_data)} bytes, expected {SAMPLE_SIZE_BYTES}")
        return None
    try:
        # Unpack using the format defined in config_file
        timestamp_ns, temp_mc, flags = struct.unpack(SAMPLE_FORMAT, raw_data)
        return timestamp_ns, temp_mc, flags
    except struct.error as e:
        print(f"Error unpacking sample data: {e}")
        return None

def start_sampling() -> None:
    """Continuously monitors and prints samples using poll."""
    print(f"Starting sampling from {DRIVER_DEV_PATH}. Press Ctrl+C to stop.")

    try:
        # Open the device in read-only mode
        fd = os.open(DRIVER_DEV_PATH, os.O_RDONLY)
    except (FileNotFoundError, PermissionError, OSError) as e:
        print(f"Error opening device {DRIVER_DEV_PATH}: {e}")
        print("Is the driver loaded and do you have permissions?")
        return

    # Create a poll object
    poller = select.poll()
    # Register the file descriptor to watch for input (POLLIN) and priority (POLLPRI)
    poller.register(fd, select.POLLIN | select.POLLPRI)

    try:
        while True:
            # Wait indefinitely for an event (-1 timeout)
            events = poller.poll(-1)

            for descriptor, event_mask in events:
                if descriptor == fd:
                    is_alert = bool(event_mask & select.POLLPRI)
                    is_readable = bool(event_mask & select.POLLIN)

                    if is_readable or is_alert: # Should generally have POLLIN if POLLPRI is set
                        try:
                            # Read the binary sample data always from offset 0
                            raw_data = os.pread(fd, SAMPLE_SIZE_BYTES,0)
                            if not raw_data:
                                print("EOF reached on device read.")
                                break # Exit loop on EOF
                            
                            # *** GET CURRENT TIME ***
                            now = datetime.now(DISPLAY_TIMEZONE)
                            
                            # Format with milliseconds and 'Z' for UTC indication
                            formatted_ts = now.strftime('%Y-%m-%dT%H:%M:%S.') + f"{now.microsecond // 1000:03d}Z"
                            # ************************

                            parsed = parse_sample(raw_data)
                            if parsed:
                                timestamp_ns, temp_mc, flags = parsed
                                #formatted_ts = format_timestamp_ns(timestamp_ns) # Optional: use driver timestamp
                                temp_c = temp_mc / 1000.0
                                # Check the actual flag from data, fallback to poll event
                                alert_flag_set = bool(flags & SIMTEMP_SAMPLE_FLAG_THRESHOLD_HI)
                                alert_status = 1 if alert_flag_set else 0

                                prefix = "ALERT " if is_alert or alert_flag_set else ""
                                print(f"{formatted_ts} temp={temp_c:.1f}C alert={alert_status} {prefix}")

                        except OSError as e:
                            print(f"Error reading from device: {e}")
                            break # Exit loop on read error
                    elif event_mask & (select.POLLERR | select.POLLHUP | select.POLLNVAL):
                         print(f"Device error/hangup event: {event_mask}")
                         break # Exit loop on device error

            else: # Inner loop finished without break
                continue # Go back to polling
            break # Exit outer loop if inner loop broke

    except KeyboardInterrupt:
        print("\nStopping sampling.")
    except Exception as e:
        print(f"\nAn unexpected error occurred: {e}")
    finally:
        # Clean up: unregister and close the file descriptor
        if 'fd' in locals() and fd >= 0:
            poller.unregister(fd)
            os.close(fd)
            print("Device closed.")