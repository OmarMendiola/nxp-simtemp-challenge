# Test Plan - NXP Simulated Temperature Sensor Driver

This document outlines the test cases designed to verify the functionality of the `nxp_simtemp` kernel module and its interaction with the user-space CLI.

## Test Environment

* **Hardware:** Linux machine (Standard Ubuntu or WSL2 environment).
* **Software:**
    * `nxp_simtemp.ko` kernel module built successfully.
    * Python 3.9+ with necessary libraries (e.g., `zoneinfo`).
    * Kernel headers or source code matching the running kernel.
    * Standard Linux utilities (`insmod`, `rmmod`, `cat`, `echo`, `dmesg`).

## Test Suites

### TS1: Basic Module Load/Unload (Manual/Scripted)

* **ID:** TS1
* **Description:** Verify the kernel module can be loaded and unloaded cleanly without errors or warnings in the kernel log (`dmesg`). This is implicitly tested by the `run_demo.sh` script.
* **Steps:**
    1.  Ensure no previous instances of `nxp_simtemp` are loaded (`lsmod | grep nxp_simtemp` should be empty).
    2.  Clear the kernel log buffer (`sudo dmesg -C`).
    3.  Load the module: `sudo insmod kernel/nxp_simtemp.ko`.
    4.  Check kernel log for errors/warnings during load: `dmesg`.
    5.  Verify device nodes (`/dev/simtemp`) and sysfs entries (`/sys/class/misc/simtemp/*`) are created.
    6.  Unload the module: `sudo rmmod nxp_simtemp`.
    7.  Check kernel log for errors/warnings during unload: `dmesg`.
    8.  Verify device nodes and sysfs entries are removed.
* **Expected Result:**
    * Module loads and unloads without any error or warning messages in `dmesg`.
    * Device nodes and sysfs entries are created upon load and removed upon unload.

### TS2: User-space Functional Tests (Automated via `test_mode.py`)

This suite covers tests implemented within the Python CLI's test mode (`user/cli/test_mode.py`). The `run_test()` function in the script executes these tests sequentially.

* **ID:** TP1 - Periodic Rate and Data Coherence Validation
* **Description:** Verify that samples are generated approximately at the configured rate (validated via the `updates` stats counter) and that the data read from the device is coherent (parsable and within expected bounds).
* **Steps (Automated within `test_mode.py`):**
    1.  **Fast Rate Test (100ms):**
        a.  Read initial `updates` count from `/sys/class/misc/simtemp/stats`.
        b.  Set `sampling_ms` to 100 via sysfs.
        c.  Wait briefly (e.g., 1s) for the configuration change to settle.
        d.  Wait for a fixed duration (e.g., 1s).
        e.  Read final `updates` count from stats.
        f.  Calculate the difference (`stats_diff_fast`). Verify it's within tolerance (10 ± 1).
        g.  Open `/dev/simtemp`. Perform a few (e.g., 3) reads using `poll()` and `read()`.
        h.  Verify each read successfully parses the `struct simtemp_sample` and the temperature value (`temp_mc`) is within a plausible range (e.g., between `THRESHOLD_MC_MIN` - 10000 and `THRESHOLD_MC_MAX` + 10000).
        i.  Close `/dev/simtemp`.
    2.  **Slow Rate Test (1000ms):**
        a.  Read initial `updates` count from stats.
        b.  Set `sampling_ms` to 1000 via sysfs.
        c.  Wait briefly (e.g., 1s) for the configuration change to settle.
        d.  Wait for a fixed duration (e.g., 2s).
        e.  Read final `updates` count from stats.
        f.  Calculate the difference (`stats_diff_slow`). Verify it's within tolerance (2 ± 1).
        g.  (Optional: Perform coherence reads similar to the fast rate test if desired).
    3.  Restore original `sampling_ms`.
* **Expected Result:**
    * The `stats_diff_fast` should be 10 ± 1.
    * The `stats_diff_slow` should be 2 ± 1.
    * All coherence reads succeed in parsing the sample structure with plausible temperature values.
    * The test passes if all conditions are met.

* **ID:** TP2 - Threshold Event Verification
* **Description:** Verify that the `POLLPRI` event is correctly generated when the simulated temperature exceeds the configured threshold. (This was the original `run_test` function).
* **Steps (Automated within `test_mode.py`):**
    1.  Read and store the original `threshold_mc` value.
    2.  Set `threshold_mc` to a very low value known to trigger alerts (e.g., `SIMTEMP_THRESHOLD_MC_MIN` = -50000 mC).
    3.  Open `/dev/simtemp`.
    4.  Use `poll()` to wait specifically for `POLLPRI` events.
    5.  Count the number of `POLLPRI` events received within a timeout period (e.g., 10 seconds). Perform a dummy `read()` after each `POLLPRI` to potentially clear the condition if necessary (driver implementation dependent).
    6.  Restore the original `threshold_mc` value.
* **Expected Result:**
    * At least a predefined number (e.g., 2) of `POLLPRI` events should be received before the timeout.
    * The test passes if the expected number of alerts is detected.

* **ID:** TP3 - Sysfs Limits Validation
* **Description:** Verify that the driver correctly enforces the defined limits for configurable sysfs attributes (`sampling_ms`, `threshold_mc`) and handles read/write permissions for `stats`. Uses limits from `nxp_simtemp_config.h`:
    * `sampling_ms`: Min `SIMTEMP_SAMPLING_MS_MIN` (100), Max `SIMTEMP_SAMPLING_MS_MAX` (60000).
    * `threshold_mc`: Min `SIMTEMP_THRESHOLD_MC_MIN` (-50000), Max `SIMTEMP_THRESHOLD_MC_MAX` (150000).
* **Steps (Automated within `test_mode.py`):**
    1.  **`sampling_ms` Limits:**
        a.  Read the current `sampling_ms` value (`original_sampling`).
        b.  Attempt to write `SIMTEMP_SAMPLING_MS_MIN - 1` (99). Verify write fails (returns error). Read back and verify value is still `original_sampling`.
        c.  Attempt to write `SIMTEMP_SAMPLING_MS_MAX + 1` (60001). Verify write fails. Read back and verify value is still `original_sampling`.
        d.  Write `SIMTEMP_SAMPLING_MS_MIN` (100). Verify write succeeds. Read back and verify value is 100.
        e.  Write `SIMTEMP_SAMPLING_MS_MAX` (60000). Verify write succeeds. Read back and verify value is 60000.
        f.  Restore `original_sampling`.
    2.  **`threshold_mc` Limits:**
        a.  Read the current `threshold_mc` value (`original_threshold`).
        b.  Attempt to write `SIMTEMP_THRESHOLD_MC_MIN - 1` (-50001). Verify write fails. Read back and verify value is still `original_threshold`.
        c.  Attempt to write `SIMTEMP_THRESHOLD_MC_MAX + 1` (150001). Verify write fails. Read back and verify value is still `original_threshold`.
        d.  Write `SIMTEMP_THRESHOLD_MC_MIN` (-50000). Verify write succeeds. Read back and verify value is -50000.
        e.  Write `SIMTEMP_THRESHOLD_MC_MAX` (150000). Verify write succeeds. Read back and verify value is 150000.
        f.  Restore `original_threshold`.
    3.  **`mode` (String):**
        a. Read current `mode` (`original_mode`).
        b. Attempt to write an invalid string (e.g., "invalid_mode"). Verify write fails. Read back and verify value is still `original_mode`.
        c. Write a valid mode (e.g., "noisy"). Verify write succeeds. Read back and verify value is "noisy".
        d. Restore `original_mode`.
    4.  **`stats` (Read-Only):**
        a.  Read `stats`. Verify read succeeds and returns a string in the expected format (e.g., "updates=X alerts=Y errors=Z").
        b.  Attempt to write any value to `stats`. Verify write fails (permission error expected).
* **Expected Result:**
    * Writes outside the defined MIN/MAX limits for numeric attributes fail.
    * Writes within or at the MIN/MAX limits succeed.
    * Reads always reflect the last successfully written value.
    * Writing an invalid string to `mode` fails.
    * Writing valid strings to `mode` succeeds.
    * Reading `stats` succeeds and shows the expected format.
    * Writing to `stats` fails.
    * The test passes if all steps behave as expected.

* **ID:** TP4 - Concurrency Test
* **Description:** Verify driver stability (no deadlocks or crashes) when sysfs configuration changes occur concurrently with device reads.
* **Steps (Automated within `test_mode.py`, using threading):**
    1.  Start a background thread that continuously reads from `/dev/simtemp` using blocking reads or `poll`/`read` in a loop for a fixed duration (e.g., 5 seconds). Log any read errors.
    2.  In the main thread, concurrently perform a series of rapid writes to sysfs attributes (`sampling_ms`, `threshold_mc`, `mode`) with valid values in a loop for the same duration.
    3.  Wait for both the reading thread and the writing loop to complete.
    4.  Check kernel log (`dmesg`) for any warnings, errors, or deadlock messages related to the driver.
* **Expected Result:**
    * The driver does not crash or deadlock.
    * No driver-specific errors or warnings appear in `dmesg`.
    * The reading thread may encounter occasional timeouts (`ETIMEDOUT`) if `sampling_ms` is changed to a large value during the test, but should not see persistent errors (-EFAULT, -ENODEV etc.).
    * The test passes if the driver remains stable and functional.

* **ID:** TP5 - API Contract - Read Offset
* **Description:** Verify that the `read()` operation on `/dev/simtemp` only supports reading from the beginning of the "file" (offset 0), as is typical for character devices providing single records or streams. Partial reads are not intended.
* **Steps (Automated within `test_mode.py`):**
    1.  Open `/dev/simtemp`.
    2.  Attempt to perform a `pread()` with an offset greater than 0 (e.g., `os.pread(fd, SAMPLE_SIZE_BYTES, 1)`).
* **Expected Result:**
    * The `pread()` call with a non-zero offset should ideally return 0 (indicating EOF, as implemented in the current driver) or potentially fail with an error like `-EINVAL` or `-ESPIPE`. It should *not* return partial data or succeed incorrectly. The current driver returns 0 for `*offp > 0`.
    * The test passes if the read behaves as expected (returns 0 bytes for non-zero offset).

* **ID:** TP6 - Mode Behavior Validation
* **Description:** Verify the correct behavior of the `ramp` and `noisy` simulation modes.
* **Steps (Automated within `test_mode.py`):**
    1.  Read and store the original `mode`.
    2.  **Ramp Mode Test:**
        a.  Set `mode` to "ramp" via sysfs.
        b.  Set `sampling_ms` to 100 (or another reasonably fast value).
        c.  Open `/dev/simtemp`.
        d.  Read ~5 consecutive samples using blocking reads or `poll`/`read`. Store the `temp_mc` value of each sample.
        e.  Verify that each sample's `temp_mc` is strictly greater than the previous sample's `temp_mc`. (Current driver increments by 100). If any sample is less than or equal to the previous, fail the test.
    3.  **Noisy Mode Test:**
        a.  Set `mode` to "noisy" via sysfs.
        b.  Set `sampling_ms` to 100.
        c.  Read ~20 consecutive samples. Keep track of the last `temp_mc` and maintain a counter (`consecutive_equal_count`) for how many times the current sample equals the last one.
        d.  For each new sample:
            i.  If `current_temp_mc == last_temp_mc`, increment `consecutive_equal_count`.
            ii. If `current_temp_mc != last_temp_mc`, reset `consecutive_equal_count` to 0.
            iii. If `consecutive_equal_count` reaches 3, fail the test immediately.
            iv. Update `last_temp_mc = current_temp_mc`.
    4.  **Cleanup:** Restore the original `mode`.
* **Expected Result:**
    * **Ramp Test:** All consecutive samples show strictly increasing temperature values.
    * **Noisy Test:** No more than 2 consecutive samples have the exact same temperature value.
    * The test passes if both mode tests complete successfully without failing their respective conditions.

## Test Execution

* **TS1:** Can be run manually or as part of `scripts/run_demo.sh`. Check `dmesg` output.
* **TS2 (TP1-TP6):** Run automatically via the Python CLI test mode:
    ```bash
    sudo python3 user/cli/main.py
    # Select option 4 ("Run Self-Test") from the menu.

    ```
    The script will print PASS/FAIL status for each TP and overall. Check `dmesg` as well for TP4.