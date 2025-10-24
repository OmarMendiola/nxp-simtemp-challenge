# test_mode.py
"""Runs automated functional tests for the simtemp driver."""

import select
import os
import time
import typing
import threading
import struct
import errno
import sys

from config_file import (
    DRIVER_DEV_PATH, TEST_PASS_CODE, TEST_FAIL_CODE, SAMPLE_SIZE_BYTES,
    SAMPLE_FORMAT
)
import configuration as conf # Import configuration functions
import print_samples # Needed for parse_sample

# --- Test Constants ---
TEST_TIMEOUT_SECONDS = 10
# TP1 Constants
TP1_SAMPLING_MS_FAST = 100
TP1_DURATION_S_FAST = 1.0
TP1_EXPECTED_COUNT_FAST = int(TP1_DURATION_S_FAST * 1000 / TP1_SAMPLING_MS_FAST)
TP1_SAMPLING_MS_SLOW = 1000
TP1_DURATION_S_SLOW = 10.0 # Wait longer to ensure multiple samples
TP1_EXPECTED_COUNT_SLOW = int(TP1_DURATION_S_SLOW * 1000 / TP1_SAMPLING_MS_SLOW)
TP1_TOLERANCE = 1
TP1_COHERENCE_READS = 3 # Number of reads to check data format
TP1_CONFIG_DELAY_S = 1.0 # Time to wait after changing config
# TP2 Constants
TP2_ALERT_THRESHOLD_MC = -50000 # A very low value to force alerts
TP2_EXPECTED_ALERTS = 2
# TP3 Constants (Mirroring kernel/nxp_simtemp_config.h)
SAMPLING_MS_MIN = 100
SAMPLING_MS_MAX = 60000
THRESHOLD_MC_MIN = -50000
THRESHOLD_MC_MAX = 150000
# TP4 Constants
TP4_DURATION_S = 5
# TP6 Constants
TP6_RAMP_SAMPLES = 5
TP6_NOISY_SAMPLES = 20
TP6_NOISY_MAX_CONSECUTIVE = 2

# --- Helper Functions ---

def _read_sample(fd: int) -> typing.Optional[typing.Tuple[int, int, int]]:
    """Reads and parses a single sample using blocking os.read()."""
    try:
        # Using simple blocking read here for simplicity
        #raw_data = os.read(fd, SAMPLE_SIZE_BYTES)
        raw_data = os.pread(fd, SAMPLE_SIZE_BYTES, 0) # Use pread if offset must be 0
        if not raw_data:
            print("ERROR: Read EOF unexpectedly.")
            return None
        return print_samples.parse_sample(raw_data)
    except OSError as e:
        # EAGAIN shouldn't happen with blocking read unless timeout set on fd
        print(f"ERROR: Failed to read sample: {e}")
        return None
    except Exception as e:
        print(f"ERROR: Unexpected error reading/parsing sample: {e}")
        return None

def _parse_stats(stats_str: typing.Optional[str]) -> typing.Dict[str, int]:
    """Parses the 'updates=X alerts=Y errors=Z' string."""
    stats = {'updates': -1, 'alerts': -1, 'errors': -1}
    if stats_str is None:
        print("ERROR: Could not read stats string.")
        return stats
    try:
        parts = stats_str.split()
        for part in parts:
            key, value = part.split('=')
            stats[key] = int(value)
    except Exception as e:
        print(f"ERROR: Could not parse stats string '{stats_str}': {e}")
        # Return dict with -1 to indicate parsing failure
        stats = {'updates': -1, 'alerts': -1, 'errors': -1}
    return stats


# --- Test Case Functions (TP1-TP6) ---

def _test_periodic_read() -> bool:
    """TP1: Verify sample rate via stats counter and data coherence."""
    print("--- Running TP1: Periodic Read Validation ---")
    passed = True
    original_sampling = None
    fd = -1
    poller = None # Use poller for coherence check reads

    try:
        original_sampling = conf.get_sampling_ms()
        if original_sampling is None:
            print("ERROR: Failed to get initial sampling rate.")
            return False

        # --- Test Fast Rate (100ms) ---
        print(f"INFO: Testing rate: {TP1_SAMPLING_MS_FAST}ms")
        if not conf.set_sampling_ms(TP1_SAMPLING_MS_FAST):
            print(f"ERROR: Failed to set sampling_ms to {TP1_SAMPLING_MS_FAST}.")
            return False
        
        time.sleep(TP1_CONFIG_DELAY_S) # Wait for propagate the config change

        initial_stats_str = conf.get_stats()
        initial_stats = _parse_stats(initial_stats_str)
        if initial_stats['updates'] < 0: return False # Error already printed

        time.sleep(TP1_DURATION_S_FAST) # Wait for samples to accumulate

        final_stats_str = conf.get_stats()
        final_stats = _parse_stats(final_stats_str)
        if final_stats['updates'] < 0: return False # Error already printed

        stats_diff_fast = final_stats['updates'] - initial_stats['updates']
        print(f"INFO: Stats 'updates' counter increased by {stats_diff_fast} in {TP1_DURATION_S_FAST}s.")

        # Verification (Fast Rate)
        stats_ok_fast = abs(stats_diff_fast - TP1_EXPECTED_COUNT_FAST) <= TP1_TOLERANCE
        if stats_ok_fast:
            print(f"INFO: Stats increment matches expected rate ({TP1_EXPECTED_COUNT_FAST}±{TP1_TOLERANCE}).")
        else:
            print(f"FAIL: Expected ~{TP1_EXPECTED_COUNT_FAST}±{TP1_TOLERANCE} updates based on stats.")
            print(f"      Stat difference: {stats_diff_fast}")
            passed = False

        # Data Coherence Check (Fast Rate)
        print("INFO: Performing data coherence read check...")
        read_ok_count = 0
        try:
            # Open in blocking mode for simple read test
            fd = os.open(DRIVER_DEV_PATH, os.O_RDONLY)
            # Add to poll object to wait for data
            poller = select.poll()
            poller.register(fd, select.POLLIN)

            for i in range(TP1_COHERENCE_READS):
                 # Wait up to 2 * sampling period for a sample
                 events = poller.poll(TP1_SAMPLING_MS_FAST * 2 + 100) # Poll with timeout
                 if not events:
                      print(f"FAIL: Timeout waiting for sample {i+1}/{TP1_COHERENCE_READS}.")
                      passed = False
                      break # Exit coherence check loop
                 # Read and parse
                 parsed_sample = _read_sample(fd)
                 if parsed_sample is not None:
                      ts, temp, flags = parsed_sample
                      print(f"INFO: Read sample {i+1}: ts={ts}, temp={temp}, flags={flags}")
                      # Basic sanity check on values (e.g., temp within reasonable range)
                      if temp < THRESHOLD_MC_MIN - 10000 or temp > THRESHOLD_MC_MAX + 10000:
                           print(f"WARN: Sample {i+1} temperature {temp} seems out of expected range.")
                           # Decide if this should be a failure depending on strictness
                           # passed = False
                           # break
                      read_ok_count += 1
                 else:
                      print(f"FAIL: Failed to read or parse sample {i+1}/{TP1_COHERENCE_READS}.")
                      passed = False
                      break # Exit coherence check loop
        except Exception as e_read:
             print(f"FAIL: Error during coherence read check: {e_read}")
             passed = False
        finally:
             if fd >= 0:
                 if poller: poller.unregister(fd)
                 os.close(fd)
                 fd = -1
                 poller = None # Reset poller

        if read_ok_count == TP1_COHERENCE_READS:
             print("INFO: Data coherence check passed.")
        else:
             print("FAIL: Data coherence check failed.")
             passed = False # Ensure overall test fails


        # --- Test Slow Rate (1000ms) ---
        print(f"\nINFO: Testing rate: {TP1_SAMPLING_MS_SLOW}ms")
        if not conf.set_sampling_ms(TP1_SAMPLING_MS_SLOW):
            print(f"ERROR: Failed to set sampling_ms to {TP1_SAMPLING_MS_SLOW}.")
            # Try restoring original before failing
            if original_sampling is not None: conf.set_sampling_ms(original_sampling)
            return False

        time.sleep(TP1_CONFIG_DELAY_S) # Wait for propagate the config change

        initial_stats_str_slow = conf.get_stats()
        initial_stats_slow = _parse_stats(initial_stats_str_slow)
        if initial_stats_slow['updates'] < 0: return False

        time.sleep(TP1_DURATION_S_SLOW) # Wait for samples

        final_stats_str_slow = conf.get_stats()
        final_stats_slow = _parse_stats(final_stats_str_slow)
        if final_stats_slow['updates'] < 0: return False

        stats_diff_slow = final_stats_slow['updates'] - initial_stats_slow['updates']
        print(f"INFO: Stats 'updates' counter increased by {stats_diff_slow} in {TP1_DURATION_S_SLOW}s.")

        # Verification (Slow Rate)
        stats_ok_slow = abs(stats_diff_slow - TP1_EXPECTED_COUNT_SLOW) <= TP1_TOLERANCE
        if stats_ok_slow:
            print(f"INFO: Stats increment matches expected rate ({TP1_EXPECTED_COUNT_SLOW}±{TP1_TOLERANCE}).")
        else:
            print(f"FAIL: Expected ~{TP1_EXPECTED_COUNT_SLOW}±{TP1_TOLERANCE} updates based on stats.")
            print(f"      Stat difference: {stats_diff_slow}")
            passed = False
        # (Optional: Add coherence check for slow rate too if desired)

    except Exception as e:
        print(f"ERROR: Unexpected exception in TP1: {e}")
        passed = False
    finally:
        if fd >= 0: # Should be closed already, but just in case
            if poller: poller.unregister(fd)
            os.close(fd)
        if original_sampling is not None:
            print(f"INFO: Restoring original sampling rate ({original_sampling}ms)...")
            conf.set_sampling_ms(original_sampling) # Restore original
        print(f"--- TP1 Result: {'PASS' if passed else 'FAIL'} ---")
        return passed

def _test_threshold_event() -> bool:
    """TP2: Verify POLLPRI alert generation."""
    print("--- Running TP2: Threshold Event Verification ---")
    original_threshold: typing.Optional[int] = None
    alert_count: int = 0
    fd: int = -1
    poller: typing.Optional[select.poll] = None
    start_time: float = time.monotonic()
    passed = False # Default to False, explicitly set True on success

    try:
        print("INFO: Getting original threshold...")
        original_threshold = conf.get_threshold_mc()
        if original_threshold is None:
            print("ERROR: Failed to get original threshold. Aborting test.")
            return False
        print(f"INFO: Original threshold: {original_threshold} mC")

        print(f"INFO: Setting threshold to {TP2_ALERT_THRESHOLD_MC} mC to trigger alerts...")
        if not conf.set_threshold_mc(TP2_ALERT_THRESHOLD_MC):
            print("ERROR: Failed to set alert threshold. Aborting test.")
            return False # Cleanup happens in finally

        print(f"INFO: Monitoring {DRIVER_DEV_PATH} for {TP2_EXPECTED_ALERTS} alerts (POLLPRI)...")
        # Use NONBLOCK for poll safety, but read logic assumes POLLPRI means data is ready
        fd = os.open(DRIVER_DEV_PATH, os.O_RDONLY | os.O_NONBLOCK)
        poller = select.poll()
        poller.register(fd, select.POLLPRI) # Only interested in priority events

        test_failed_explicitly = False
        while alert_count < TP2_EXPECTED_ALERTS:
            elapsed_time = time.monotonic() - start_time
            if elapsed_time > TEST_TIMEOUT_SECONDS:
                print(f"ERROR: Test timed out after {TEST_TIMEOUT_SECONDS} seconds.")
                print(f"      Detected {alert_count}/{TP2_EXPECTED_ALERTS} alerts.")
                test_failed_explicitly = True
                break # Exit while loop

            remaining_ms = max(1, int((TEST_TIMEOUT_SECONDS - elapsed_time) * 1000))
            events = poller.poll(remaining_ms)

            if not events: continue

            for descriptor, event_mask in events:
                if descriptor == fd:
                    if event_mask & select.POLLPRI:
                        alert_count += 1
                        print(f"INFO: ALERT DETECTED! ({alert_count}/{TP2_EXPECTED_ALERTS})")
                        # Perform read to potentially clear condition in driver
                        # Read using pread with offset 0 and NONBLOCK flag
                        try:
                            _ = os.pread(fd, SAMPLE_SIZE_BYTES, 0)
                        except BlockingIOError:
                            print("WARN: Read after POLLPRI blocked (EAGAIN), might be OK.")
                        except OSError as e_read:
                            print(f"WARN: Error reading after POLLPRI: {e_read}")
                            # Decide if this is critical failure
                    elif event_mask & (select.POLLERR | select.POLLHUP | select.POLLNVAL):
                         print(f"ERROR: Device error during poll: mask={event_mask}. Aborting.")
                         test_failed_explicitly = True
                         break # Exit inner loop
            if test_failed_explicitly: break # Exit outer loop if inner loop broke

        # Validation
        if not test_failed_explicitly and alert_count >= TP2_EXPECTED_ALERTS:
            print(f"INFO: Success: Detected {alert_count} alerts.")
            passed = True
        else:
             # Already printed timeout or device error message
             passed = False


    except Exception as e:
         print(f"ERROR: Unexpected exception in TP2: {e}")
         passed = False
    finally:
        # Cleanup
        if fd >= 0:
            if poller: poller.unregister(fd)
            os.close(fd)
        if original_threshold is not None:
            print(f"INFO: Restoring original threshold ({original_threshold} mC)...")
            if not conf.set_threshold_mc(original_threshold):
                print("ERROR: Failed to restore original threshold!")
                passed = False # Mark as fail if cleanup fails
        print(f"--- TP2 Result: {'PASS' if passed else 'FAIL'} ---")
        return passed

def _test_sysfs_limits() -> bool:
    """TP3: Verify sysfs attribute limits and permissions."""
    print("--- Running TP3: Sysfs Limits Validation ---")
    passed = True
    original_sampling = None
    original_threshold = None
    original_mode = None

    try:
        # --- Get originals ---
        original_sampling = conf.get_sampling_ms()
        original_threshold = conf.get_threshold_mc()
        original_mode = conf.get_mode()
        if original_sampling is None or original_threshold is None or original_mode is None:
            print("ERROR: Failed to get initial config.")
            return False

        print("INFO: Testing sampling_ms limits...")
        # Below min
        print("INFO: Testing sampling_ms below minimum...")
        if conf.set_config_value(conf.SAMPLING_MS_PATH, str(SAMPLING_MS_MIN - 1)):
            print("FAIL: Wrote value below sampling_ms min!")
            passed = False
        current = conf.get_sampling_ms()
        if current != original_sampling:
            print(f"FAIL: sampling_ms changed after invalid write (is {current}, expected {original_sampling})!")
            passed = False
        # Above max
        print("INFO: Testing sampling_ms above maximum...")
        if conf.set_config_value(conf.SAMPLING_MS_PATH, str(SAMPLING_MS_MAX + 1)):
            print("FAIL: Wrote value above sampling_ms max!")
            passed = False
        current = conf.get_sampling_ms()
        if current != original_sampling:
            print(f"FAIL: sampling_ms changed after invalid write (is {current}, expected {original_sampling})!")
            passed = False
        # At min
        print("INFO: Testing sampling_ms at minimum...")
        if not conf.set_config_value(conf.SAMPLING_MS_PATH, str(SAMPLING_MS_MIN)): passed = False; print("FAIL: Write at sampling_ms min failed!")
        current = conf.get_sampling_ms()
        if current != SAMPLING_MS_MIN: passed = False; print(f"FAIL: sampling_ms min check failed (read {current})")
        # At max
        print("INFO: Testing sampling_ms at maximum...")
        if not conf.set_config_value(conf.SAMPLING_MS_PATH, str(SAMPLING_MS_MAX)): passed = False; print("FAIL: Write at sampling_ms max failed!")
        current = conf.get_sampling_ms()
        if current != SAMPLING_MS_MAX: passed = False; print(f"FAIL: sampling_ms max check failed (read {current})")
        # Restore original sampling_ms
        if not conf.set_sampling_ms(original_sampling): print("WARN: Failed to restore original sampling_ms"); passed = False


        print("INFO: Testing threshold_mc limits...")
         # Below min
        if conf.set_config_value(conf.THRESHOLD_MC_PATH, str(THRESHOLD_MC_MIN - 1)): passed = False; print("FAIL: threshold below min")
        if conf.get_threshold_mc() != original_threshold: passed = False; print("FAIL: threshold changed after invalid write (min)")
        # Above max
        if conf.set_config_value(conf.THRESHOLD_MC_PATH, str(THRESHOLD_MC_MAX + 1)): passed = False; print("FAIL: threshold above max")
        if conf.get_threshold_mc() != original_threshold: passed = False; print("FAIL: threshold changed after invalid write (max)")
        # At min
        if not conf.set_config_value(conf.THRESHOLD_MC_PATH, str(THRESHOLD_MC_MIN)): passed = False; print("FAIL: Write at threshold min failed!")
        current = conf.get_threshold_mc()
        if current != THRESHOLD_MC_MIN: passed = False; print(f"FAIL: threshold min check failed (read {current})")
        # At max
        if not conf.set_config_value(conf.THRESHOLD_MC_PATH, str(THRESHOLD_MC_MAX)): passed = False; print("FAIL: Write at threshold max failed!")
        current = conf.get_threshold_mc()
        if current != THRESHOLD_MC_MAX: passed = False; print(f"FAIL: threshold max check failed (read {current})")
        # Restore original threshold
        if not conf.set_threshold_mc(original_threshold): print("WARN: Failed to restore original threshold"); passed = False

        print("INFO: Testing mode limits...")
        # Invalid string
        if conf.set_config_value(conf.MODE_PATH, "invalid_mode"): passed = False; print("FAIL: invalid mode write succeeded")
        current_mode = conf.get_mode()
        if current_mode != original_mode: passed = False; print(f"FAIL: mode changed after invalid write (is {current_mode})")
        # Valid string
        valid_mode = "noisy" if original_mode != "noisy" else "ramp" # Pick a different valid one
        if not conf.set_config_value(conf.MODE_PATH, valid_mode): passed = False; print(f"FAIL: valid mode write ('{valid_mode}') failed!")
        current_mode = conf.get_mode()
        if current_mode != valid_mode: passed = False; print(f"FAIL: valid mode check failed (read {current_mode}, expected {valid_mode})")
        # Restore original mode
        if not conf.set_mode(original_mode): print("WARN: Failed to restore original mode"); passed = False

        print("INFO: Testing stats read/write permissions...")
        stats_str = conf.get_stats()
        if stats_str is None or '=' not in stats_str:
            print(f"FAIL: Could not read stats or format is wrong: {stats_str}")
            passed = False
        else:
             print(f"INFO: Read stats OK: {stats_str}")
             stats_dict = _parse_stats(stats_str)
             if stats_dict['updates'] < 0: # Check if parsing failed
                 passed = False

        # Attempt write using low-level function which returns bool
        if conf.set_config_value(conf.STATS_PATH, "test"):
            print("FAIL: Writing to stats succeeded (should be read-only)!")
            passed = False
        else:
            print("INFO: Writing to stats failed as expected.")

    except Exception as e:
        print(f"ERROR: Unexpected exception in TP3: {e}")
        passed = False
    finally:
        # Ensure originals are restored if they were read successfully and not already restored
        # This double-checks restoration in case of early exit from try block
        current_sampling = conf.get_sampling_ms()
        if original_sampling is not None and current_sampling != original_sampling:
             print(f"INFO: Restoring original sampling_ms ({original_sampling}) in finally block...")
             conf.set_sampling_ms(original_sampling)
        current_threshold = conf.get_threshold_mc()
        if original_threshold is not None and current_threshold != original_threshold:
             print(f"INFO: Restoring original threshold ({original_threshold}) in finally block...")
             conf.set_threshold_mc(original_threshold)
        current_mode = conf.get_mode()
        if original_mode is not None and current_mode != original_mode:
             print(f"INFO: Restoring original mode ('{original_mode}') in finally block...")
             conf.set_mode(original_mode)
        print(f"--- TP3 Result: {'PASS' if passed else 'FAIL'} ---")
        return passed


def _reader_thread_func(stop_event: threading.Event, results: dict):
    """Thread function for concurrent reading in TP4."""
    fd = -1
    poller = None
    read_count = 0
    errors = 0
    try:
        # Use NONBLOCK for poll safety
        fd = os.open(DRIVER_DEV_PATH, os.O_RDONLY | os.O_NONBLOCK)
        poller = select.poll()
        poller.register(fd, select.POLLIN)
        while not stop_event.is_set():
            # Poll with short timeout to make thread responsive to stop_event
            events = poller.poll(50) # 50ms timeout
            if not events: continue # Timeout, check stop_event again

            for descriptor, event_mask in events:
                 if descriptor == fd:
                      if event_mask & select.POLLIN:
                           # Attempt to read using pread (offset 0)
                           try:
                                raw_data = os.pread(fd, SAMPLE_SIZE_BYTES, 0)
                                if raw_data:
                                    read_count += 1
                                    # Optional: parse_sample(raw_data) to check coherence
                                else:
                                    print("READER THREAD: Read EOF") # Should not happen unless driver unloaded
                                    errors += 1
                                    stop_event.set()
                           except BlockingIOError:
                                # EAGAIN is expected with O_NONBLOCK + pread if no new data
                                pass
                           except OSError as e_read:
                                print(f"READER THREAD: Read error: {e_read}")
                                errors += 1
                                stop_event.set()
                      elif event_mask & (select.POLLERR | select.POLLHUP | select.POLLNVAL):
                           print("READER THREAD: Device error/hangup during poll!")
                           errors += 1
                           stop_event.set() # Signal main thread to stop
                           break # Exit inner loop
            else: # Inner loop finished without break
                 continue
            break # Exit outer loop if inner loop broke (device error)
    except Exception as e:
        print(f"READER THREAD: Unexpected exception: {e}")
        errors += 1
        stop_event.set() # Ensure main thread stops on error
    finally:
        if fd >= 0:
            if poller: poller.unregister(fd)
            os.close(fd)
        results['read_count'] = read_count
        results['errors'] = errors
        print(f"READER THREAD: Finished. Reads={read_count}, Errors={errors}")


def _test_concurrency() -> bool:
    """TP4: Test stability with concurrent reads and config writes."""
    print("--- Running TP4: Concurrency Test ---")
    passed = True
    original_sampling = None
    original_threshold = None
    original_mode = None

    # Get initial config first to ensure driver is accessible
    try:
        original_sampling = conf.get_sampling_ms()
        original_threshold = conf.get_threshold_mc()
        original_mode = conf.get_mode()
        if original_sampling is None or original_threshold is None or original_mode is None:
            print("ERROR: Failed to get initial config. Cannot start TP4.")
            return False
    except Exception as e:
         print(f"ERROR: Failed getting initial config for TP4: {e}")
         return False


    stop_event = threading.Event()
    reader_results = {'read_count': 0, 'errors': 0}
    reader_thread = None # Define variable outside try block

    try:
        reader_thread = threading.Thread(target=_reader_thread_func, args=(stop_event, reader_results))

        print(f"INFO: Starting reader thread and writer loop for {TP4_DURATION_S}s...")
        reader_thread.start()

        start_time = time.monotonic()
        modes = ["normal", "noisy", "ramp"]
        i = 0
        writer_errors = 0
        while time.monotonic() - start_time < TP4_DURATION_S:
             if stop_event.is_set(): # Check if reader thread had an error
                  print("WRITER: Stop event set, terminating writer loop.")
                  break
             # Rapidly change configuration
             if not conf.set_sampling_ms(SAMPLING_MS_MIN + (i % 10) * 10): writer_errors+=1
             if not conf.set_threshold_mc(i * 1000 % (THRESHOLD_MC_MAX - THRESHOLD_MC_MIN) + THRESHOLD_MC_MIN): writer_errors+=1
             if not conf.set_mode(modes[i % len(modes)]): writer_errors+=1
             time.sleep(0.02) # Small delay between writes
             i += 1
             if i % 50 == 0: print("WRITER: Still writing...") # Progress indicator

        print("INFO: Stopping reader thread...")
        stop_event.set()
        reader_thread.join(timeout=2) # Wait max 2s for thread to finish

        if reader_thread.is_alive():
            print("FAIL: Reader thread did not terminate!")
            passed = False

        # Check for errors reported by the reader thread (non-EAGAIN OS errors)
        if reader_results['errors'] > 0:
            print(f"FAIL: Reader thread reported {reader_results['errors']} persistent errors.")
            passed = False
        else:
             print("INFO: Reader thread reported 0 persistent errors.")

        # Check for errors during sysfs writes
        if writer_errors > 0:
             print(f"FAIL: Writer encountered {writer_errors} errors during sysfs writes.")
             passed = False
        else:
             print("INFO: Writer reported 0 errors during sysfs writes.")

        # Check dmesg might be needed manually if issues suspected
        print("INFO: Test finished. Check dmesg for kernel warnings/errors (not checked automatically).")

    except Exception as e:
        print(f"ERROR: Unexpected exception in TP4: {e}")
        passed = False
    finally:
        # Ensure originals are restored
        if stop_event and not stop_event.is_set(): stop_event.set() # Ensure thread stops if error occurred before
        if reader_thread and reader_thread.is_alive(): reader_thread.join(timeout=1) # Give it one last chance
        if original_sampling is not None: conf.set_sampling_ms(original_sampling)
        if original_threshold is not None: conf.set_threshold_mc(original_threshold)
        if original_mode is not None: conf.set_mode(original_mode)
        print(f"--- TP4 Result: {'PASS' if passed else 'FAIL'} ---")
        return passed

def _test_api_contract_offset() -> bool:
    """TP5: Verify read() fails or returns EOF for non-zero offset."""
    print("--- Running TP5: API Contract - Read Offset ---")
    passed = False
    fd = -1
    try:
        fd = os.open(DRIVER_DEV_PATH, os.O_RDONLY)
        # Attempt pread with offset 1
        read_bytes = os.pread(fd, SAMPLE_SIZE_BYTES, 1)

        # Driver's current implementation returns 0 (EOF) for offset > 0
        if len(read_bytes) == 0:
             print("INFO: pread() with offset > 0 returned 0 bytes (EOF) as expected.")
             passed = True
        else:
             print(f"FAIL: pread() with offset > 0 returned {len(read_bytes)} bytes (expected 0).")
             # Potentially print the data if needed for debug: print(read_bytes)

    except OSError as e:
        # Some drivers might return EINVAL or ESPIPE, consider this acceptable too
        if e.errno == errno.EINVAL or e.errno == errno.ESPIPE:
            print(f"INFO: pread() with offset > 0 failed with {errno.errorcode[e.errno]} as expected.")
            passed = True
        else:
            print(f"ERROR: pread() failed unexpectedly: {e}")
    except Exception as e:
        print(f"ERROR: Unexpected exception in TP5: {e}")
    finally:
        if fd >= 0: os.close(fd)
        print(f"--- TP5 Result: {'PASS' if passed else 'FAIL'} ---")
        return passed


def _test_mode_behavior() -> bool:
    """TP6: Verify 'ramp' and 'noisy' mode behavior."""
    print("--- Running TP6: Mode Behavior Validation ---")
    passed = True
    original_mode = None
    original_sampling = None
    fd = -1
    poller = None

    try:
        original_mode = conf.get_mode()
        original_sampling = conf.get_sampling_ms()
        if original_mode is None or original_sampling is None:
            print("ERROR: Failed to get initial config.")
            return False

        # --- Ramp Test ---
        print("INFO: Testing 'ramp' mode...")
        ramp_ok = True # Assume success for this part
        try:
             if not conf.set_mode("ramp"): raise ValueError("Failed to set ramp mode")
             if not conf.set_sampling_ms(TP1_SAMPLING_MS_FAST): raise ValueError("Failed to set sampling ms") # Use faster sampling

             fd = os.open(DRIVER_DEV_PATH, os.O_RDONLY) # Blocking read is fine here
             poller = select.poll()
             poller.register(fd, select.POLLIN)

             last_temp_mc = -float('inf') # Initialize lower than any possible value
             samples_read = 0
             while samples_read < TP6_RAMP_SAMPLES:
                 events = poller.poll(TP1_SAMPLING_MS_FAST * 3) # Timeout slightly longer than sampling
                 if not events:
                     print("FAIL: Timeout waiting for ramp sample.")
                     ramp_ok = False; break
                 # Read sample
                 parsed = _read_sample(fd)
                 if parsed is None:
                     ramp_ok = False; break
                 _ts, current_temp_mc, _flags = parsed
                 print(f"RAMP Check: Read {current_temp_mc} (last was {last_temp_mc})")
                 # First sample just sets the baseline
                 if samples_read > 0:
                     # Driver increments by 100
                     expected_temp = last_temp_mc + 100
                     # Handle potential wrap-around if driver implements it
                     if last_temp_mc > 99000 and current_temp_mc < 1000: # Heuristic for wrap
                          print("INFO: Ramp wrap-around detected (expected).")
                     elif current_temp_mc <= last_temp_mc:
                          print(f"FAIL: Ramp mode failed. Current ({current_temp_mc}) <= Previous ({last_temp_mc}).")
                          ramp_ok = False
                          break
                 last_temp_mc = current_temp_mc
                 samples_read += 1
        except Exception as e_ramp:
             print(f"ERROR during Ramp test setup or execution: {e_ramp}")
             ramp_ok = False
        finally:
             if fd >= 0 :
                  if poller: poller.unregister(fd)
                  os.close(fd)
                  fd = -1; poller = None # Reset for next test

        if not ramp_ok: passed = False


        # --- Noisy Test ---
        print("INFO: Testing 'noisy' mode...")
        noisy_ok = True # Assume success for this part
        try:
             if not conf.set_mode("noisy"): raise ValueError("Failed to set noisy mode")
             if not conf.set_sampling_ms(TP1_SAMPLING_MS_FAST): raise ValueError("Failed to set sampling ms") # Keep faster sampling

             fd = os.open(DRIVER_DEV_PATH, os.O_RDONLY)
             poller = select.poll()
             poller.register(fd, select.POLLIN)

             last_temp_mc = -float('inf')
             consecutive_equal_count = 0
             samples_read = 0
             while samples_read < TP6_NOISY_SAMPLES:
                 events = poller.poll(TP1_SAMPLING_MS_FAST * 3) # Timeout
                 if not events:
                     print("FAIL: Timeout waiting for noisy sample.")
                     noisy_ok = False; break
                 # Read sample
                 parsed = _read_sample(fd)
                 if parsed is None:
                     noisy_ok = False; break
                 _ts, current_temp_mc, _flags = parsed

                 if samples_read > 0: # Only compare after first sample
                     if current_temp_mc == last_temp_mc:
                         consecutive_equal_count += 1
                     else:
                         consecutive_equal_count = 0 # Reset counter
                     print(f"NOISY Check: Read {current_temp_mc} (last was {last_temp_mc})")

                     # print(f"NOISY Check: Read {current_temp_mc} (last was {last_temp_mc}), consecutive: {consecutive_equal_count}") # Debug
                     if consecutive_equal_count >= TP6_NOISY_MAX_CONSECUTIVE:
                         print(f"FAIL: Noisy mode failed. Got {TP6_NOISY_MAX_CONSECUTIVE + 1} consecutive equal samples ({current_temp_mc}).")
                         noisy_ok = False
                         break
                 last_temp_mc = current_temp_mc
                 samples_read += 1
        except Exception as e_noisy:
             print(f"ERROR during Noisy test setup or execution: {e_noisy}")
             noisy_ok = False
        finally:
             if fd >= 0:
                  if poller: poller.unregister(fd)
                  os.close(fd)
                  fd = -1; poller = None

        if not noisy_ok: passed = False


    except Exception as e:
        print(f"ERROR: Unexpected exception in TP6 setup or cleanup: {e}")
        passed = False
    finally:
        # Restore originals only once at the end
        if original_mode is not None: conf.set_mode(original_mode)
        if original_sampling is not None: conf.set_sampling_ms(original_sampling)
        print(f"--- TP6 Result: {'PASS' if passed else 'FAIL'} ---")
        return passed


# --- Main Test Runner ---

def run_all_tests() -> int:
    """Runs all TP tests sequentially."""
    print("\n========= Starting Simtemp Driver Test Suite =========")

    # Basic check for root/sudo - needed even when called as module
    if os.geteuid() != 0:
        print("ERROR: Test suite must be run with root privileges (sudo).")
        # Returning fail code instead of sys.exit
        return TEST_FAIL_CODE

    # Check if driver seems loaded - needed even when called as module
    if not os.path.exists(DRIVER_DEV_PATH) or not os.path.exists(conf.MODE_PATH):
        print(f"ERROR: Driver node {DRIVER_DEV_PATH} or sysfs path not found.")
        print("       Please ensure the nxp_simtemp module is loaded.")
        # Returning fail code instead of sys.exit
        return TEST_FAIL_CODE


    # List of test functions to run
    test_functions = [
        _test_periodic_read,
        _test_threshold_event,
        _test_sysfs_limits,
        _test_concurrency,
        _test_api_contract_offset,
        _test_mode_behavior,
    ]

    results = {}
    overall_passed = True

    for test_func in test_functions:
        test_name = test_func.__name__.replace("_test_", "").upper() # Get TP name like TP1
        try:
             # Add extra newline for separation
             print(f"\n----- Executing {test_name} -----")
             passed = test_func()
             results[test_name] = passed
             if not passed:
                 overall_passed = False
        except Exception as e:
             print(f"CRITICAL ERROR: Exception escaped test function {test_name}: {e}")
             results[test_name] = False
             overall_passed = False
        # print("-" * 50) # Separator removed, handled by test function prints


    # --- Summary ---
    print("\n========= Test Suite Summary =========")
    for name, result in results.items():
        print(f"{name}: {'PASS' if result else 'FAIL'}")

    print("====================================")
    final_result = TEST_PASS_CODE if overall_passed else TEST_FAIL_CODE
    print(f"Overall Result: {'PASS' if final_result == TEST_PASS_CODE else 'FAIL'}")
    print("====================================")
    return final_result


if __name__ == "__main__":
    # Basic check for root/sudo
    if os.geteuid() != 0:
        print("ERROR: This test script must be run with root privileges (sudo).")
        sys.exit(TEST_FAIL_CODE)

    # Check if driver seems loaded (basic check)
    if not os.path.exists(DRIVER_DEV_PATH) or not os.path.exists(conf.MODE_PATH):
        print(f"ERROR: Driver node {DRIVER_DEV_PATH} or sysfs path not found.")
        print("       Please ensure the nxp_simtemp module is loaded (`sudo insmod kernel/nxp_simtemp.ko`).")
        sys.exit(TEST_FAIL_CODE)

    # Run the tests
    exit_code = run_all_tests()
    sys.exit(exit_code)