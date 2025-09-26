```
   _____ ______ ______ _______          _ 
  / ____|  ____|  ____|__   __|        | |
 | (___ | |__  | |__     | | ___   ___ | |
  \___ \|  __| |  __|    | |/ _ \ / _ \| |
  ____) | |____| |       | | (_) | (_) | |
 |_____/|______|_|       |_|\___/ \___/|_|                     
```
                                          
# SEFTool / SE Flash Tool

A command-line utility for flashing, unlocking, and managing Sony Ericsson phones (A1 platform) over a serial connection.  
Supports firmware flashing, GDFS (phone data) backup/restore, flash reading, and basic unlock operations.

---

## Features

- Identify connected Sony Ericsson phone
- Flash main and filesystem firmware
- Read raw flash memory
- Backup and restore GDFS (phone settings/data)
- Unlock phone (usercode, simlock planned)
- Cross-platform (Linux/Windows, built with CMake + libserialport)

---

## Building

### Prerequisites
- CMake **3.10+**
- C compiler:
  - **Linux/macOS**: GCC or Clang
  - **Windows**: MinGW (recommended)
- [libserialport](https://sigrok.org/wiki/Libserialport) installed and available  
  - **Debian/Ubuntu**: `sudo apt install libserialport-dev`  
  - **Fedora**: `sudo dnf install libserialport-devel`  
  - **Arch Linux**: `sudo pacman -S libserialport`  
  - **Windows**: `pacman -S mingw-w64-x86_64-libserialport`

### Build Steps
```sh
git clone https://github.com/farid1991/seftool.git
cd seftool
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

---

## Usage
```sh
Usage: ./seftool -p <port> [-b <baudrate>] -a <action>
```
```
Options:
  -p, --port <name>       Serial port name (e.g. COM2, /dev/ttyUSB0)
  -b, --baud <rate>       Baudrate (default: 115200)
  -a, --action <action>   Action:
                          identify
                          flash <main> xor <fs>
                          read-flash start <addr> size <bytes> OR block <count>
                            [save-as-babe]
                          read-gdfs
                          write-gdfs <filename>
                          write-script <filename>
                          unlock <usercode|simlock>
Global options:
    --anycid              Ignore CID restrictions (DB2012/DB2020/PNX5230)
  -h, --help              Show this help message
```

---

## Supported devices

| Action         | DB2000           | DB201x                | DB2020         | PNX5230 |
|:---------------|:----------------:|:---------------------:|:--------------:|:-------:|
| identify       | OK               | OK                    | OK             | OK      |
| flash          | OK (except Z1010)| OK                    | OK             | OK      |
| read-flash     | CID29&36,BROWN49 | CID29&36+BROWN+ANYCID | BROWN49+ANYCID | ANYCID  |
| read-gdfs      | OK               | OK                    | OK             | OK      |
| write-gdfs     | OK               | OK                    | OK             | OK      |
| write-script   | OK               | OK                    | OK             | OK      |
| unlock-usercode| OK               | OK                    | OK             | OK      |
| unlock-simlock | TODO             | TODO                  | TODO           | TODO    |


---

## Examples

### Identify phone (identify)
Backup security units on first operation.
```sh
$ ./seftool -p COM2 -b 921600 -a identify
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
$ ./seftool -p /dev/ttyUSB0 -b 921600 -a flash main.mbn fs.fbn
```

#### Flash main only:
```sh
$ ./seftool -p /dev/ttyUSB0 -b 921600 -a flash main.mbn
```

#### Flash filesystem only:
```sh
$ ./seftool -p /dev/ttyUSB0 -b 921600 -a flash fs.fbn
```

### Read flash (read-flash):

#### Read 512 KB starting at 0x44000000 and save as RAW (.bin):
```sh
$ ./seftool -p COM2 -b 921600 -a read-flash start 0x44000000 size 0x80000
```

#### Read 0x10 blocks starting at 0x20100000 with anycid:
```sh
$ ./seftool -p COM2 -b 921600 -a read-flash start 0x20100000 block 0x10 --anycid
```

#### Read 0x40000 bytes starting at 0x20100000 and save as BABE (.ssw):
```sh
seftool -p COM2 -b 921600 -a read-flash start 0x20100000 size 0x40000 save-as-babe --anycid
```

### Backup GDFS (read-gdfs):
```sh
$ ./seftool -p COM2 -b 921600 -a read-gdfs
```

### Restore GDFS from file (write-gdfs):
```sh
$ ./seftool -p COM2 -b 921600 -a write-gdfs backup/gdfs.bin
```

### Write GDFS script (write-script):
```sh
$ ./seftool -p COM2 -b 921600 -a write-script secsunitbackup.txt
```

### Unlock usercode:
```sh
$ ./seftool -p /dev/ttyUSB0 b 921600 -a unlock usercode
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
* Sim unlock is not implemented yet.
* Use a stable power supply(battery 50%++) and reliable serial connection when flashing to avoid bricking the phone.
* **DB2000** has max **460800** baudrate.
* **Z1010** flash MAIN is incorrect.
* **PNX5230** read flash use `anycid` method, restore firmware is not implemented yet.
* **ANYCID** only used for "read-flash" right now.
* Restore firmware files are included in `./rest` (packaged alongside the executable).  
  `.rest` files are generated with **mkrest2** by **den_po**.
---

## TODO
1. CDA upload (flash customization).
2. Complete operation for phone with CID16 and ~~CID29~~ (DB2000 & DB2010)
3. VKP patching
4. ...

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
MIT License – free to use and modify.
