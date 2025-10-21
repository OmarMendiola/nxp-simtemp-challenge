# test_mode.py
"""Runs an automated self-test for the simtemp driver's alert mechanism."""

import select
import os
import time
import typing

from config_file import (
    DRIVER_DEV_PATH, TEST_PASS_CODE, TEST_FAIL_CODE, SAMPLE_SIZE_BYTES
)
import configuration as conf # Import configuration functions

# Constants for the test
TEST_ALERT_THRESHOLD_MC = -50000 # A very low value to force alerts
TEST_EXPECTED_ALERTS = 2
TEST_TIMEOUT_SECONDS = 10

def run_test() -> int:
    """Runs a self-test to verify high-priority alert generation.

    1. Saves original threshold.
    2. Sets threshold very low (-50 C).
    3. Uses poll() to wait for 2 POLLPRI events.
    4. Restores original threshold.
    5. Returns TEST_PASS_CODE (0) on success, TEST_FAIL_CODE (-1) on timeout/error.
    """
    print("--- Starting Self-Test ---")
    original_threshold: typing.Optional[int] = None
    alert_count: int = 0
    fd: int = -1
    poller: typing.Optional[select.poll] = None
    start_time: float = time.monotonic()
    result_code: int = TEST_FAIL_CODE # Assume failure

    try:
        # 1. Setup: Get and save original threshold
        print("Getting original threshold...")
        original_threshold = conf.get_threshold_mc()
        if original_threshold is None:
            print("Error: Failed to get original threshold. Aborting test.")
            return TEST_FAIL_CODE
        print(f"Original threshold: {original_threshold} mC")

        # 2. Trigger: Set threshold low
        print(f"Setting threshold to {TEST_ALERT_THRESHOLD_MC} mC to trigger alerts...")
        if not conf.set_threshold_mc(TEST_ALERT_THRESHOLD_MC):
            print("Error: Failed to set alert threshold. Aborting test.")
            # Attempt to restore original threshold even on failure
            if original_threshold is not None:
                print("Attempting to restore original threshold...")
                conf.set_threshold_mc(original_threshold)
            return TEST_FAIL_CODE

        # 3. Detect: Monitor for POLLPRI events
        print(f"Monitoring {DRIVER_DEV_PATH} for {TEST_EXPECTED_ALERTS} alerts (POLLPRI)...")
        fd = os.open(DRIVER_DEV_PATH, os.O_RDONLY | os.O_NONBLOCK) # Open non-block for safety
        poller = select.poll()
        poller.register(fd, select.POLLPRI) # Only interested in priority events

        while alert_count < TEST_EXPECTED_ALERTS:
            # Check for timeout
            elapsed_time = time.monotonic() - start_time
            if elapsed_time > TEST_TIMEOUT_SECONDS:
                print(f"Error: Test timed out after {TEST_TIMEOUT_SECONDS} seconds.")
                print(f"Detected {alert_count}/{TEST_EXPECTED_ALERTS} alerts.")
                result_code = TEST_FAIL_CODE
                break # Exit while loop

            # Calculate remaining timeout for poll (minimum 1ms)
            remaining_ms = max(1, int((TEST_TIMEOUT_SECONDS - elapsed_time) * 1000))

            # Poll for priority events with timeout
            events = poller.poll(remaining_ms)

            if not events:
                # Poll timed out for this iteration, continue loop check
                continue

            for descriptor, event_mask in events:
                if descriptor == fd:
                    if event_mask & select.POLLPRI:
                        alert_count += 1
                        print(f"ALERT DETECTED! ({alert_count}/{TEST_EXPECTED_ALERTS})")
                        # Read data to clear the condition (important!)
                        # We don't need to parse it for the test itself.
                        try:
                             _ = os.pread(fd, SAMPLE_SIZE_BYTES,0)
                        except OSError as e:
                             # Ignore EAGAIN if we happened to hit it non-blockingly
                             if e.errno != errno.EAGAIN:
                                 print(f"Warning: Error reading after POLLPRI: {e}")

                    elif event_mask & (select.POLLERR | select.POLLHUP | select.POLLNVAL):
                         print(f"Device error during poll: mask={event_mask}. Aborting.")
                         result_code = TEST_FAIL_CODE
                         # Break inner loop
                         break
            else: # Inner loop finished without break
                 continue # Continue outer loop

            break # Exit outer loop if inner loop broke (device error)


        # 4. Validate: Check if expected alerts were received before timeout
        if alert_count >= TEST_EXPECTED_ALERTS:
            print(f"Success: Detected {alert_count} alerts.")
            result_code = TEST_PASS_CODE

    except (FileNotFoundError, PermissionError, OSError) as e:
        print(f"Error during test execution: {e}")
        result_code = TEST_FAIL_CODE
    except Exception as e:
         print(f"An unexpected error occurred: {e}")
         result_code = TEST_FAIL_CODE
    finally:
        # 5. Cleanup: Always try to restore the original threshold
        if original_threshold is not None:
            print(f"Restoring original threshold ({original_threshold} mC)...")
            if not conf.set_threshold_mc(original_threshold):
                print("Error: Failed to restore original threshold!")
                # Mark test as failed if cleanup fails
                result_code = TEST_FAIL_CODE

        # Close device file descriptor if open
        if fd >= 0:
             if poller:
                 poller.unregister(fd)
             os.close(fd)
             print("Test device closed.")

        print(f"--- Self-Test Finished (Result: {'PASS' if result_code == TEST_PASS_CODE else 'FAIL'}) ---")
        return result_code