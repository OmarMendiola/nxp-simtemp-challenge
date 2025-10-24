# main.py
"""CLI entry point for interacting with the simtemp driver."""

import sys
import configuration as conf
import print_samples
import test_mode
from config_file import TEST_PASS_CODE
import os

def display_menu():
    """Prints the main menu options."""
    print("\n--- Simtemp Driver CLI ---")
    print("1. View Current Configuration")
    print("2. Modify Configuration")
    print("3. Start Periodic Sampling (Ctrl+C to stop)")
    print("4. Run Self-Test")
    print("5. Exit")
    print("-------------------------")

def view_configuration():
    """Retrieves and prints all current configuration values."""
    print("\n--- Current Configuration ---")
    sampling = conf.get_sampling_ms()
    threshold = conf.get_threshold_mc()
    mode = conf.get_mode()
    stats = conf.get_stats()

    print(f"Sampling Period (ms): {sampling if sampling is not None else 'Error reading'}")
    print(f"Alert Threshold (mC): {threshold if threshold is not None else 'Error reading'}")
    print(f"Simulation Mode       : {mode if mode is not None else 'Error reading'}")
    print(f"Statistics            : {stats if stats is not None else 'Error reading'}")
    print("---------------------------")

def modify_configuration():
    """Provides a sub-menu to change driver settings."""
    while True:
        print("\n--- Modify Configuration ---")
        print("1. Set Sampling Period (ms)")
        print("2. Set Alert Threshold (mC)")
        print("3. Set Mode (normal|noisy|ramp)")
        print("4. Back to Main Menu")
        print("--------------------------")
        choice = input("Enter choice: ")

        if choice == '1':
            try:
                value = int(input("Enter new sampling period (ms): "))
                if not conf.set_sampling_ms(value):
                    print("Failed to set value (check permissions or dmesg).")
            except ValueError:
                print("Invalid input. Please enter an integer.")
        elif choice == '2':
            try:
                value = int(input("Enter new alert threshold (mC): "))
                if not conf.set_threshold_mc(value):
                     print("Failed to set value (check permissions or dmesg).")
            except ValueError:
                print("Invalid input. Please enter an integer.")
        elif choice == '3':
            value = input("Enter new mode (normal|noisy|ramp): ").lower()
            if not conf.set_mode(value):
                 print("Failed to set value (check permissions or dmesg).")
        elif choice == '4':
            break
        else:
            print("Invalid choice.")

def run_cli():
    """Runs the main interactive command-line interface loop."""
    while True:
        display_menu()
        choice = input("Enter choice: ")

        if choice == '1':
            view_configuration()
        elif choice == '2':
            modify_configuration()
        elif choice == '3':
            print_samples.start_sampling()
        elif choice == '4':
            result = test_mode.run_all_tests()
            if result == TEST_PASS_CODE:
                print(">>> Test PASSED <<<")
            else:
                print(">>> Test FAILED <<<")
        elif choice == '5':
            print("Exiting.")
            break
        else:
            print("Invalid choice, please try again.")

if __name__ == "__main__":
    # Basic check for root/sudo, as sysfs writes often require it
    if os.geteuid() != 0:
        print("Warning: Running without root privileges.")
        print("Modifying configuration or running tests might fail due to permissions.")
        # input("Press Enter to continue anyway, or Ctrl+C to exit...")

    run_cli()
    sys.exit(0)