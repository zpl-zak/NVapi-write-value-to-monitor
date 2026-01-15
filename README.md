# NVapi-write-value-to-monitor
Send commands to monitor over i2c. <br>
This can be used to issue VCP commands or other manufacturer specific commands.

**Platform Support:**
- **Windows**: Uses NVIDIA API (NVAPI)
- **Linux**: Uses ddcutil (works with any GPU)


This program relies on the NVIDIA API (NVAPI), to compile it you will need to download the api which can be found here: <br> https://developer.nvidia.com/rtx/path-tracing/nvapi/get-started

### History 
This program was created after discovering that my display does not work with <b>ControlMyMonitor</b> to change inputs using VCP commands. Searching for an antlernative lead me to this thread https://github.com/rockowitz/ddcutil/issues/100 where other users had found a way to switch the inputs of their LG monitors using a linux program, I needed a windows solution. That lead to the NVIDIA API, this program is an adaptation of the i2c example code provided in the API

## Usage

### Syntax
```
writeValueToDisplay.exe <display_index> <input_value> <command_code> [register_address]
```

| Argument | Description |
| -------- | ----------- |
| display_index | Index assigned to monitor by OS (Typically 0 for first screen, try running "mstsc.exe /l" in command prompt to see how windows has indexed your display(s)) |
| input_value   | value to write to screen |
| command_code  | VCP code or other|
| register_address | Address to write to, default 0x51 for VCP codes |



## Example Usage
Change display 0 brightness to 50% using VCP code 0x10
```
writeValueToDisplay.exe 0 0x32 0x10 
```
<br>

Change display 0 input to HDMI 1 using VCP code 0x60 on supported displays
```
writeValueToDisplay.exe 0 0x11 0x60 
```

### Change input on some displays
Some displays do not support using VCP codes to change inputs. I have tested this using values from this thread https://github.com/rockowitz/ddcutil/issues/100 with my LG Ultragear 27GP850-B. Your milage may vary with other monitors, <b>use at your own risk!</b>

#### Change input to HDMI 1 on LG Ultragear 27GP850-B
NOTE: LG Ultragear 27GP850-B is display 0 for me
```
writeValueToDisplay.exe 0 0x90 0xF4 0x50
```

#### Change input to Displayport on LG Ultragear 27GP850-B
NOTE: LG Ultragear 27GP850-B is display 0 for me
```
writeValueToDisplay.exe 0 0xD0 0xF4 0x50
```

---

## Linux Version

The Linux version uses [ddcutil](https://www.ddcutil.com/) as the backend and works with any GPU (NVIDIA, AMD, Intel).

### Dependencies

```bash
# Debian/Ubuntu
sudo apt install ddcutil i2c-tools

# Fedora
sudo dnf install ddcutil i2c-tools

# Arch
sudo pacman -S ddcutil i2c-tools
```

### User Permissions

You must be in the `i2c` group to access the I2C bus:

```bash
sudo usermod -aG i2c $USER
```

Log out and back in for the group change to take effect.

### Building

```bash
cd linux
make
```

### Usage

The Linux version has the same CLI syntax as Windows:

```bash
./writeValueToDisplay <display_index> <input_value> <command_code> [register_address]
```

Use `-1` for `display_index` to auto-detect the primary display.

### Linux Examples

Change display 0 brightness to 50%:
```bash
./writeValueToDisplay 0 0x32 0x10
```

Change input to HDMI 1 on LG Ultragear:
```bash
./writeValueToDisplay 0 0x90 0xF4 0x50
```

Change input to DisplayPort on LG Ultragear:
```bash
./writeValueToDisplay 0 0xD0 0xF4 0x50
```

Auto-detect primary display:
```bash
./writeValueToDisplay -1 0xD0 0xF4 0x50
```
