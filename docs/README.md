# NXP Simulated Temperature Sensor Linux Driver

This project implements a Linux kernel module that simulates a temperature sensor device. It provides a character device interface (`/dev/simtemp`) for reading temperature samples and a sysfs interface (`/sys/class/misc/simtemp/`) for configuration and statistics. A user-space Python CLI application is included for interacting with the driver.

**Git Repository:** [text](https://github.com/OmarMendiola/nxp-simtemp-challenge)
**Demo Video:** [text](https://www.youtube.com/watch?v=bRTNjQ5id-U)

## Features

* **Kernel Module:** Simulates temperature readings with different modes (normal, noisy, ramp).
* **Character Device:** `/dev/simtemp` provides blocking and non-blocking reads for binary temperature samples (`struct simtemp_sample`). Supports `poll()` for efficient waiting.
* **Sysfs Interface:** Located at `/sys/class/misc/simtemp/`, allows viewing and changing:
    * `sampling_ms`: Update period in milliseconds.
    * `threshold_mc`: Alert threshold in milli-Celsius.
    * `mode`: Simulation mode (`normal`, `noisy`, `ramp`).
    * `stats`: Read-only view of updates, alerts, and errors.
* **User-Space CLI:** A Python application (`user/cli/main.py`) provides an interactive menu to:
    * View/Modify configuration via sysfs.
    * Monitor temperature samples from the character device.
    * Run an automated self-test of the alert mechanism.
* **Build Scripts:** Includes scripts for building (`build.sh`), running a demo (`run_demo.sh`), and linting (`lint.sh`). Handles standard Linux and WSL2 environments.

## Prerequisites

1.  **Linux Environment:** A Linux machine (tested on Ubuntu) or WSL2.
2.  **Build Tools:** `make`, `gcc`, and potentially other tools required for kernel module compilation. Install using:
    ```bash
    sudo apt-get update
    sudo apt-get install build-essential
    ```
3.  **Kernel Headers or Source:**
    * **Standard Linux:** The headers corresponding to your *exact* running kernel version must be installed. Usually done via:
        ```bash
        sudo apt-get install linux-headers-$(uname -r)
        ```
    * **WSL2:** The standard headers are *not* typically in the Ubuntu repositories. You need to clone the specific Microsoft WSL2 kernel source, check out the correct tag, and prepare it for module building. The `build.sh` script will guide you through this if it detects a WSL2 environment and the source is missing.
4.  **Python 3:** The user-space CLI application requires Python 3.9 or newer (due to the use of the `zoneinfo` library for timezones). Install Python 3 if it's not already present:
    ```bash
    sudo apt-get install python3 python3-pip
    ```
    *(Note: `tzdata` might also be needed for `zoneinfo`: `sudo apt-get install tzdata`)*

## Build Instructions

The provided build script automates the process of finding kernel headers/source and compiling the module.

1.  **Navigate to the `scripts` directory:**
    ```bash
    cd scripts
    ```
2.  **Run the build script:**
    ```bash
    bash build.sh
    ```
    * The script will first check for standard kernel headers at `/lib/modules/$(uname -r)/build`.
    * If running on **WSL2** and standard headers are not found, it will look for the cloned kernel source at `~/WSL2-Linux-Kernel`.
    * If **WSL2 source is needed but not found**, the script will **stop** and provide the **exact commands** you need to run to clone the repository, check out the correct tag, and prepare it (`make modules_prepare`). After completing those steps, re-run `bash build.sh`.
    * If **headers/source are missing on standard Linux** or **build tools are missing**, the script will provide hints on how to install them using `apt-get`.

Upon successful completion, the kernel module `kernel/nxp_simtemp.ko` will be built. The user-space application (`user/cli/main.py`) is a Python script and does not require separate compilation.

## Running the Demo

The `run_demo.sh` script provides a full cycle test: loading the module, configuring it, running the user-space self-test, and unloading the module. **It requires root privileges.**

1.  **Navigate to the `scripts` directory (if not already there):**
    ```bash
    cd scripts
    ```
2.  **Run the demo script using `sudo` and `bash`:**
    ```bash
    sudo bash run_demo.sh
    ```

The script will:
* Load `nxp_simtemp.ko`.
* Verify device creation (`/dev/simtemp`, `/sys/class/misc/simtemp`).
* Set the simulation mode to `noisy` via sysfs.
* Run the Python CLI application's self-test option (which takes ~10 seconds).
* Automatically unload the module upon completion or error (using `trap`).

## User-Space CLI Usage

You can interact with the driver manually using the Python CLI application. **Note that accessing the device node (`/dev/simtemp`) and the sysfs entries usually requires root privileges.**

1.  **Load the kernel module (if not already loaded):**
    ```bash
    sudo insmod kernel/nxp_simtemp.ko
    ```
2.  **Run the CLI application using `sudo`:**
    ```bash
    sudo python3 user/cli/main.py
    ```
    *(Running with `sudo` is generally necessary for reading `/dev/simtemp` and reading/writing sysfs attributes.)*

The CLI presents a menu:
* **View Configuration:** Reads and displays current settings from sysfs.
* **Modify Configuration:** Allows changing `sampling_ms`, `threshold_mc`, and `mode` via sysfs.
* **Start Periodic Sampling:** Reads from `/dev/simtemp` using `poll()` and prints new samples as they arrive. Press `Ctrl+C` to stop.
* **Run Self-Test:** Executes an automated test to verify the alert mechanism using `poll()` with `POLLPRI`.
* **Exit:** Quits the CLI application.

3.  **Unload the module when finished:**
    ```bash
    sudo rmmod nxp_simtemp
    ```

## Code Quality (Optional)

The `lint.sh` script attempts to run `checkpatch.pl` (from kernel source), `clang-format`, and `flake8` if they are available on your system.

```bash
bash scripts/lint.sh