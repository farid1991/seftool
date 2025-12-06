```
███████╗███████╗███████╗████████╗ ██████╗  ██████╗ ██╗
██╔════╝██╔════╝██╔════╝╚══██╔══╝██╔═══██╗██╔═══██╗██║
███████╗█████╗  █████╗     ██║   ██║   ██║██║   ██║██║
╚════██║██╔══╝  ██╔══╝     ██║   ██║   ██║██║   ██║██║
███████║███████╗██║        ██║   ╚██████╔╝╚██████╔╝███████╗
╚══════╝╚══════╝╚═╝        ╚═╝    ╚═════╝  ╚═════╝ ╚══════╝
```

# SEFTool / SE Flash Tool

A command-line utility for flashing, unlocking, and managing Sony Ericsson AVR & ARM phones (DB1000, DB2000, DB2010, DB2012, DB2020, PNX5230) over a serial connection.
Supports firmware flashing and patching, GDFS (phone data) backup/restore, flash reading, and basic unlock operations.

---

## Features

- Identify connected Sony Ericsson phone
- Flash main and filesystem firmware
- Read raw flash memory
- Backup and restore GDFS (phone settings/data)
- Unlock phone (usercode, simlock planned)
- Patch firmware
- Cross-platform (Linux/Windows, built with CMake + libserialport)

---

## Building

### Prerequisites
- CMake **3.10+**
- C compiler:
  - **Linux/macOS**: GCC or Clang
  - **Windows**: MinGW (recommended)

### Dependencies
Required runtime/build libraries:
- [zlib](https://zlib.net/)
- [bzip2](https://sourceware.org/bzip2/)
- [minizip](https://github.com/domoticz/minizip)
- [libserialport](https://sigrok.org/wiki/Libserialport)

### Install packages per platform:

- **Debian / Ubuntu**
```sh
sudo apt update
sudo apt install build-essential cmake pkg-config libserialport-dev zlib1g-dev libbz2-dev libminizip-dev
```

- **Fedora**
```sh
sudo dnf install gcc make cmake pkg-config libserialport-devel zlib-devel bzip2-devel minizip-devel
```

- **Arch Linux**
```sh
sudo pacman -S --needed base-devel cmake libserialport zlib bzip2 minizip
```

- **macOS (Homebrew)**
```sh
brew install cmake pkg-config libserialport zlib bzip2 minizip
```

- **Windows (MSYS2 / MinGW64)**
```sh
pacman -Syu
pacman -S --needed mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake \
       mingw-w64-x86_64-libserialport \
       mingw-w64-x86_64-zlib mingw-w64-x86_64-bzip2 mingw-w64-x86_64-minizip
```

### Build Steps
#### Basic build steps (works on Linux/macOS and MSYS2 MinGW when using the appropriate shell):
```sh
git clone https://github.com/farid1991/seftool.git
cd seftool
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

#### Windows (MSYS2 MinGW64) — recommended in the MinGW64 shell:
```sh
git clone https://github.com/farid1991/seftool.git
cd seftool
mkdir build && cd build
/mingw64/bin/cmake -G "MinGW Makefiles" ..
mingw32-make
```

---

### Linux Serial Port Permissions

On Linux systems (Armbian, Debian, Ubuntu, etc.), accessing `/dev/ttyUSB*`
or `/dev/ttyACM*` often requires being in the **dialout** group.
If you see: sp_open failed: Permission denied
Fix it by adding your user to the dialout group:

```bash
sudo usermod -aG dialout $USER
```

## Usage
```sh
Usage: ./seftool -p <port> [-b <baudrate>] -a <action>
```
### Options:
```
    -p, --port <name>                    Serial port name (e.g. COM2, /dev/ttyUSB0)
    -b, --baud <rate>                    Baudrate (default: 115200)
    -a, --action <action>                Specify an action (see below)

General action:
    identify                             Identify phone and show basic info

Ericsson
Platform: Z80
Available actions:
    identify                             Identify phone and show basic info
    read-eeprom                          Backup EEPROM area
    write-eeprom <eeprom_file>           Restore EEPROM area

Platform: AVR
Available actions:
    identify                             Identify phone and show basic info

Sony Ericsson
Platform: pre-A1 (DB1000)
Available actions:
    identify                             Identify phone and show basic info
    flash-avr <avr_file>                 Flash AVR MCU firmware
    flash-arm <arm_file>                 Flash ARM/DSP firmware

Platform: A1 (DB2000 / DB2010 / DB2012 / DB2020 / PNX5230)
Available actions:
    identify                                             Identify phone and show basic info
    flash [main <file>] [fs <file>] [cda <file>]         Flash MAIN / FS firmware and/or upload CDA
    read-flash start <addr> size <bytes> | block <count> Read flash (specify start & size or block count)
    read-gdfs                                            Backup GDFS area
    write-gdfs <filename>                                Restore GDFS from file
    write-script <file1> [file2 ...]                     Apply VKP or GDFS script(s)
    unlock <usercode|simlock>                            Unlock user code or SIM lock
    fsx-upload src <local...> [dest <fs path>]           Upload files to internal FS
    fsx-download src <fs path>/<file> [dest <local>]     Download files from internal FS
    upload-anycid                                        Upload AnyCID package for exploit access

Utility actions:
    convert babe2raw <filename>          Convert BABE -> raw
    convert raw2babe <filename> <addr>   Convert raw -> BABE at address

A1 Global options:
    --anycid                             Ignore CID restrictions (DB2012/DB2020/PNX5230)
    --break-rsa                          Break RSA on DB2000 & DB2010 RED49

General options:
    -h, --help                           Show this help message
```
## Platform-specific action summary

#### Platform: pre-A1 (DB1000 — chip IDs 0x5807, 0x5808)
* identify — Identify phone and show basic info
* flash-avr <avr_file> — Flash AVR MCU firmware
* flash-arm <arm_file> — Flash ARM/DSP firmware

#### Platform: A1 (DB2000 / DB2010 / DB2012 / DB2020 / PNX5230)
* identify — Identify phone and show basic info
* flash [main <file>] [fs <file>] [cda <file>] — Flash MAIN / FS firmware and/or upload CDA
* read-flash start <addr> size <bytes> | block <count> — Read flash (specify start & size or block count)
* read-gdfs — Backup GDFS area
* write-gdfs <filename> — Restore GDFS from file
* write-script <file1> [file2 ...] — Apply VKP or GDFS script(s)
* unlock <usercode|simlock> — Unlock user code or SIM lock
* fsx-upload src <local...> [dest <fs path>] — Upload files to internal FS
* fsx-download src <fs path>/<file> [dest <local>] — Download files from internal FS
* upload-anycid — Upload AnyCID package for exploit access

#### Utility actions
* convert babe2raw <filename> — Convert BABE -> raw
* convert raw2babe <filename> <addr> — Convert raw -> BABE at address
---

## Supported devices

| Action             | DB2000        | DB201x                | DB2020         | PNX5230 |
|:-------------------|:-------------:|:---------------------:|:--------------:|:-------:|
| identify           | OK            | OK                    | OK             | OK      |
| flash              | OK            | OK                    | OK             | OK      |
| read-flash         | CID29,36,49   | CID29,36,49 + ANYCID  | BROWN49+ANYCID | ANYCID  |
| read-gdfs          | OK            | OK                    | OK             | OK      |
| write-gdfs         | OK            | OK                    | OK             | OK      |
| write-script[gdfs] | OK~~CID29~~   | OK                    | OK             | OK      |
| write-script[vkp]  | CID36,49      | CID29,36,49 + ANYCID  | BROWN49+ANYCID | ANYCID  |
| unlock-usercode    | OK            | OK                    | OK             | OK      |
| unlock-simlock     | TODO          | TODO                  | TODO           | TODO    |
| upload-anycid      | NOTHING       | OK                    | OK             | OK      |
| fsx-upload         | OK            | OK                    | OK             | OK      |
| fsx-download       | OK~~CID29~~   | CID29,36,49 + ANYCID  | BROWN49+ANYCID | ANYCID  |


---

## Examples

### Identify phone (identify)
Backup security units on first operation.
```sh
$ .\seftool.exe -p COM2 -b 921600 -a identify
```
<details>
<summary>output</summary>

```sh
Port: COM2
Baudrate: 921600
Action: identify

Powering phone
Waiting for reply (30s timeout):
30 seconds remaining...
Connected

Detected Sony Ericsson
Chip ID: 9900, Platform: DB2020
EMP Protocol: 03.01
SPEED: 921600

LDR: 071130 1150 NPACXC1250330_DB2020_PRODUCTIONIDLOADER_P3M
FLASH ID: 0x897e (Intel)
OTP: LOCKED:1 CID:51 PAF:1 IMEI:35958401******
ACTIVE CID:53 COLOR:RED
Activating GDFS.. activated

Phone Info (from GDFS):
Model: W660i
Brand: Sony Ericsson
MAPP CXC article: R8BB001     prgCXC1250446_GENERIC_FY
MAPP CXC version: R8BB001
Language Package: C_ASIA
CDA article: CDA102568/7
CDA revision: R8A
Default article: cxc1250839
Default version: R6AD001
SIMLOCKS NOT DETECTED
Provider: 000-000

Shutdown phone
Done
```
</details>

### Flashing firmware (flash):
Support only firmware with BABE format. bin, ssw, mbn, fbn.

#### Flash main + filesystem firmware:
```sh
$ ./seftool -p /dev/ttyUSB0 -b 921600 -a flash main main.mbn fs fs.fbn
```

#### Flash main + filesystem firmware + CDA upload:
```sh
$ ./seftool -p /dev/ttyUSB0 -b 921600 -a flash main main.mbn fs fs.fbn cda cda.zip
```

#### Flash main only:
```sh
$ ./seftool -p /dev/ttyUSB0 -b 921600 -a flash main main.mbn
```

#### Flash ARM firmware (DB1000):
```sh
$ ./seftool -p /dev/ttyUSB0 -b 115200 -a flash-arm T610_R6C005_TAE.arm
```

#### Flash AVR firmware (DB1000):
```sh
$ ./seftool -p /dev/ttyUSB0 -b 115200 -a flash-avr T610_R6C005_ASIAN_LAT_1_EN-ID-MS.bin
```

#### Flash filesystem only:
```sh
$ ./seftool -p /dev/ttyUSB0 -b 921600 -a flash fs fs.fbn
```

#### Flash (upload) CDA only:
```sh
$ ./seftool -p /dev/ttyUSB0 -b 921600 -a flash cda cda.zip
```

#### Cross-flash DB201x CID49 (example: K310 → W200)
Some DB201x phones (e.g. K310) can be flashed with firmware from a different model (e.g. W200).
This requires enabling RSA-break, otherwise the flash is rejected due to CID mismatch.

```sh
$ ./seftool -p /dev/ttyUSB0 -b 921600 -a flash main w200_main.mbn fs w200_fs.fbn cda w200_cda.zip --break-rsa
```

### Read flash (read-flash):

#### Read 512 KB starting at 0x44000000 and save as RAW (.bin):
```sh
$ .\seftool.exe -p COM2 -b 921600 -a read-flash start 0x44000000 size 0x80000
```

#### Read 0x10 blocks starting at 0x20100000 with anycid:
```sh
$ .\seftool.exe -p COM2 -b 921600 -a read-flash start 0x20100000 block 0x10 --anycid
```

#### Read 0x40000 bytes starting at 0x20100000 and save as BABE (.ssw):
```sh
seftool -p COM2 -b 921600 -a read-flash start 0x20100000 size 0x40000 save-as-babe --anycid
```

### Backup GDFS (read-gdfs):
```sh
$ .\seftool.exe -p COM2 -b 921600 -a read-gdfs
```

### Restore GDFS from file (write-gdfs):
```sh
$ .\seftool.exe -p COM2 -b 921600 -a write-gdfs ./backup/gdfs.bin
```

### Write GDFS script (write-script):
```sh
$ .\seftool.exe -p COM2 -b 921600 -a write-script secsunitbackup.txt
```

### Write VKP patch (write-script):
```sh
$ .\seftool.exe -p COM2 -b 921600 -a disable_setup_wizard.vkp no_simcard.vkp
```

### Write VKP patch with anycid exploit (write-script):
```sh
$ .\seftool.exe -p COM2 -b 921600 -a disable_setup_wizard.vkp no_simcard.vkp --anycid
```

### Unlock usercode:
```sh
# Linux
./seftool -p /dev/ttyUSB0 -b 921600 -a unlock usercode

# Windows:
.\seftool.exe -p COM2 -b 921600 -a unlock usercode
```

### Upload filesystem (fsx-upload)

#### Upload a local directory or a zip archive to the phone filesystem.

- If `dest` is provided, files are placed under that phone directory.
- If `dest` is `/` (default), the tool preserves the relative path inside the source directory.
- You may pass a single directory or a single `.zip` file as the source.

##### Upload a local directory into a specific phone directory:
```sh
# Upload local directory "./Z530" into /tpa/preset/system/language on the phone
.\seftool.exe -p COM2 -b 921600 -a fsx-upload src ./Z530 dest /tpa/preset/system/language
```
##### Upload a local file(s) into a specific phone directory:
```sh
# Upload local file "./file1.txt" "../file2.txt" into /tpa/user/other on the phone
.\seftool.exe -p COM2 -b 921600 -a fsx-upload src ./file1.txt ../file2.txt dest /tpa/user/other
```
##### Upload a ZIP archive and extract its contents on the fly (dest defaults to root):
```sh
# Extract ZIP to ./tmp then upload files preserving internal paths (default dest is root '/')
.\seftool.exe -p COM2 -b 921600 -a fsx-upload src ./Z530i_CDA102462_75_R1A_Generic_Trade_Indonesia.zip
```
##### Upload from local directory but place all files directly under the given destination:
```sh
# Put the contents of ./lang directly into /tpa/preset/system/language
./seftool -p /dev/ttyUSB0 -b 921600 -a fsx-upload src ./lang dest /tpa/preset/system/language
```

### Upload anycid bypass package (upload-anycid) : auto detection, do identify first
```sh
./seftool -p /dev/ttyUSB0 -b 921600 -a upload-anycid
```

### Download filesystem (fsx-download)

##### Download an internal FS directory into a specific local directory:
```sh
.\seftool.exe -p COM2 -b 921600 -a fsx-download src /tpa/preset/system/language' dest ./lng
```

##### Download an internal FS file(s) into a specific local directory:
```sh
.\seftool.exe -p COM2 -b 921600 -a fsx-download src /tpa/preset/system/settings/splash' dest ./startup
```

### Convert firmware(BABE format) to RAW binary:
```sh
$ ./seftool -a convert babe2raw Z310_R8BA024_prgCXC1250594_GENERIC_AL.PNX5230_CID53_RED.mbn
```

### Convert RAW binary to BABE format:
```sh
$ ./seftool -a convert raw2babe prgCXC1250594_GENERIC_AL.bin 0x20100000
```

---

## Disclaimer
This software is provided **"as is"**, without any warranty of any kind.
Using `seftool` may permanently damage your device if used incorrectly.

- Always make backups before writing to flash or GDFS.
- The authors and contributors are **not responsible** for any data loss, malfunction, or device damage caused by this tool.
- Interruption during flashing or GDFS operations will not permanently brick the phone, but it may leave it in a corrupted state until reflashed.
- Use at your own risk.

---

## Notes
* Do identify first before other operation
* Backups are automatically saved in a backup/ directory when using identify, read-gdfs, write-gdfs, or read-flash.
* Use a stable power supply(battery 50%++) and reliable serial connection when flashing to avoid bricking the phone.
* **DB2000** has max **460800** baudrate.
* Restore firmware files are included in `./rest` (packaged alongside the executable).
  `.rest` files are generated with **mkrest2** by **den_po**.
* Anycid package files are included in `./anycid` (packaged alongside the executable).
* Only support bin firmware for **flash-avr** right now.
* **flash-arm** only support max baudrate **115200**
* AVR devices only support firmware with raw/bin format.
---

## TODO
1. ~~CDA upload (flash customization).~~
2. Complete operation for phone with CID16 and ~~CID29~~ (DB2000 & DB2010)
3. ~~VKP patching~~
4. ~~Break RSA RED49 DB2000 & DB2010~~
5. Patch DB2000 CID29
6. ~~Read file(s) / directory from FS~~
7. Implement *mkrest* action
8. SIM unlock by patch
9. Fix SBN Flash on Ericsson and early SE AVR devices
10. ...

---

## Contributing
Contributions are welcome!
You can help by:
- Reporting bugs or issues via GitHub Issues
- Adding support for new chipsets or features
- Improving documentation and usage examples
- Submitting pull requests with bug fixes or enhancements

To contribute:
1. Fork the repository
2. Create a new branch for your feature or fix
3. Make your changes and add tests if applicable
4. Submit a pull request for review

Please follow existing code style (C99, consistent formatting, warnings enabled).

---

## Credits
- [**the_laser**](https://support.setool.net/) – setool creator
- [**den_po**](https://github.com/justdanpo) - jdflasher creator
- **phoneXS team** - xs++ creator
- **fixeria** - help with gdfs format structure
- **B-Con** – [crypto-algorithms](https://github.com/B-Con/crypto-algorithms) (SHA1 implementation)

## License
[MIT License](LICENSE) – free to use and modify.
