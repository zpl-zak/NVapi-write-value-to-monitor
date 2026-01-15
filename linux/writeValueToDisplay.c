/*
 * writeValueToDisplay - Linux version
 *
 * Sends DDC/CI commands to monitors using ddcutil.
 * CLI-compatible with the Windows NVAPI version.
 *
 * Dependencies: ddcutil, i2c-tools
 * User must be in 'i2c' group: sudo usermod -aG i2c $USER
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MAX_CMD_LEN 512
#define MAX_LINE_LEN 256

/*
 * Detect primary display using xrandr and map to ddcutil display number.
 * Returns 1-based display number, or 1 as fallback.
 */
int detect_primary_display(void) {
    FILE *fp;
    char line[MAX_LINE_LEN];
    char primary_output[64] = "";

    // Find primary output name via xrandr
    fp = popen("xrandr 2>/dev/null | grep ' connected primary'", "r");
    if (!fp) {
        fprintf(stderr, "Failed to run xrandr, using display 1\n");
        return 1;
    }

    if (fgets(line, sizeof(line), fp)) {
        // Extract output name (first word)
        sscanf(line, "%63s", primary_output);
    }
    pclose(fp);

    if (strlen(primary_output) == 0) {
        printf("Primary display not found, defaulting to display 1\n");
        return 1;
    }

    printf("Primary display device found: %s\n", primary_output);

    // Map output name to ddcutil display number
    fp = popen("ddcutil detect 2>/dev/null", "r");
    if (!fp) {
        fprintf(stderr, "Failed to run ddcutil detect, using display 1\n");
        return 1;
    }

    int current_display = 0;
    while (fgets(line, sizeof(line), fp)) {
        // Look for "Display N" lines
        if (strncmp(line, "Display ", 8) == 0) {
            sscanf(line, "Display %d", &current_display);
        }
        // Check if this display's DRM connector matches our primary
        if (current_display > 0 && strstr(line, primary_output)) {
            pclose(fp);
            printf("Using display index %d for primary display\n", current_display - 1);
            return current_display;
        }
    }

    pclose(fp);
    printf("Could not map %s to ddcutil display, using display 1\n", primary_output);
    return 1;
}

/*
 * Execute ddcutil command and return exit status.
 */
int run_ddcutil(const char *cmd) {
    int result = system(cmd);
    if (result == -1) {
        fprintf(stderr, "Failed to execute command\n");
        return 1;
    }
    return WEXITSTATUS(result);
}

/*
 * Write value to monitor via ddcutil.
 *
 * display_num: 1-based ddcutil display number
 * input_value: value to write (0x00-0xFF)
 * command_code: VCP code or manufacturer command
 * register_address: I2C register (0x51 for standard VCP, 0x50 for LG custom)
 */
int write_value_to_monitor(int display_num, uint8_t input_value,
                            uint8_t command_code, uint8_t register_address) {
    char cmd[MAX_CMD_LEN];
    int result;

    if (register_address == 0x51) {
        // Standard VCP command
        snprintf(cmd, sizeof(cmd),
            "ddcutil -d %d setvcp x%02X x%02X",
            display_num, command_code, input_value);
    } else {
        // Manufacturer-specific command (e.g., LG with register 0x50)
        // Use --i2c-source-addr for custom register address
        snprintf(cmd, sizeof(cmd),
            "ddcutil -d %d setvcp x%02X x00%02X "
            "--i2c-source-addr=x%02X --noverify --permit-unknown-feature",
            display_num, command_code, input_value, register_address);
    }

    result = run_ddcutil(cmd);
    if (result != 0) {
        fprintf(stderr, "  ddcutil command failed with status %d\n", result);
        return 0;  // FALSE
    }

    return 1;  // TRUE
}

void print_usage(void) {
    printf("Incorrect Number of arguments!\n\n");

    printf("Arguments:\n");
    printf("display_index   - Index assigned to monitor (0 for first screen, -1 for primary)\n");
    printf("input_value     - value to write to screen (hex)\n");
    printf("command_code    - VCP code or other (hex)\n");
    printf("register_address - Address to write to, default 0x51 for VCP codes (hex)\n\n");

    printf("Usage:\n");
    printf("writeValueToDisplay [display_index] [input_value] [command_code]\n");
    printf("OR\n");
    printf("writeValueToDisplay [display_index] [input_value] [command_code] [register_address]\n");
}

int main(int argc, char *argv[]) {
    int display_index = 0;
    uint8_t input_value = 0;
    uint8_t command_code = 0;
    uint8_t register_address = 0x51;

    // Usage: writeValueToDisplay [display_index] [input_value] [command_code]
    // Uses default register address 0x51 used for VCP codes
    if (argc == 4) {
        display_index = atoi(argv[1]);
        input_value = (uint8_t)strtol(argv[2], NULL, 16);
        command_code = (uint8_t)strtol(argv[3], NULL, 16);
    }
    // Usage: writeValueToDisplay [display_index] [input_value] [command_code] [register_address]
    else if (argc == 5) {
        display_index = atoi(argv[1]);
        input_value = (uint8_t)strtol(argv[2], NULL, 16);
        command_code = (uint8_t)strtol(argv[3], NULL, 16);
        register_address = (uint8_t)strtol(argv[4], NULL, 16);
    }
    else {
        print_usage();
        return 1;
    }

    // Convert display_index to ddcutil display number (1-based)
    int ddcutil_display;
    if (display_index == -1) {
        ddcutil_display = detect_primary_display();
    } else {
        ddcutil_display = display_index + 1;
    }

    int result = write_value_to_monitor(ddcutil_display, input_value,
                                         command_code, register_address);
    if (!result) {
        printf("Changing value failed\n");
        return 1;
    }

    printf("\n");
    return 0;
}
