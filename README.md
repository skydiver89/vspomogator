**Languages**  
* [English](README.md)
* [Русский](README-RU.md)
![Vspomogator V1](./photos/assemblied.jpg)
## Description  
**Vspomogator** — this is programmable macrokeyboard with display, which connecting to PC by USB (via virtual COM-port). Device allows to run commands, emulate text typing or pressing key combinations. Buttons grouped in layouts. Display shows current layout or sytsem info (CPU temperature and load, RAM usage, network activity, battery status). 11 buttons totally. Buttons are enumerated from 1 to 11 from left to right, from top to bottom.  
## Possibilities
- **2 modes**:
  - **Layouts mode**: shows button names according to configuration file.  
  ![Layouts](./photos/layouts.jpg)
  - **System info mode**: shows CPU load (common and by cores), RAM usage, CPU temperature, network activity (Tx/Rx) and battery status.  
  ![System info](./photos/cpu.jpg)
- **Mode changing** is performing by pressing button 11.  
- **Layout changing** is performing by long pressing button 11.  
- **Reboot** is performing by long pressing buttons 1+10.  
## Materials used  
- **ESP32** DevKitV1 with CP2102 chip - some China clone. https://ozon.ru/t/zLtqDSa
- **TFT LCD display 240x320** on ST7789 chip. https://ozon.ru/t/WcsNvLD
- **11 tactile buttons** https://ozon.ru/t/3Qol6bx
- **breadboard** 12x8 https://ozon.ru/t/QWNoiOu
- **wires**
- **pin headers**. The display comes with a pin header for only one side. https://ozon.ru/t/dwWQEG1
- **device housing** is 3d-printed. Models are in *3d models* directory  
## Pinout  
### Display  
| ESP32 | ST7789 |
|-------|--------|
| 3V3   | VDD,BL |
| GND   | GND    |
| D2    | DC     |
| D4    | RES    |
| D5    | CS     |
| D18   | SCL    |
| D23   | SDA    |
### Buttons  
One pin of each button is connected to the ESP32 GND. The second pins:
| ESP32   | Button |
|---------|--------|
| D13     | 1      |
| D14     | 2      |
| D16(RX2)| 3      |
| D17(TX2)| 4      |
| D19     | 5      |
| D21     | 6      |
| D22     | 7      |
| D25     | 8      |
| D26     | 9      |
| D27     | 10     |
| D33     | 11     |
## Element placing  
If you want to print the same housing, you need to place elements on breadboard exactly like in the photo:  
![board](./photos/board.png)  
## Software  
### ESP32 firmware  
- In Arduino IDE choose board **ESP32 Dev Module**.
- Install libraries **TFT_eSPI**, **EncButton**, **ArduinoJson**.
- In library **TFT_eSPI** in file *UserSetup.h* (in my linux it`s location is *~/Arduino/libraries/TFT_eSPI/User_Setup.h*) change defines, as in file *User_Setup.h* in this repository. Or just replace the file.
- flash the ESP32.
### Service  
#### Requirements  
Out of the box it will work only in linux. For Windows it is should be modificated.  
#### Installation  
There is builded binary in relises for linux x86_64 and deb-package.  
There is also a systemd unit-file *service/buildscripts/vspomogator.service*. When the deb-package is installed, the systemd service will be installed and activated automatically, and the configuration file will be located at */etc/vspomogator/vspomogator.yml*.  
#### Building from source  
The Go compiler must be installed. Run the following in the directory *service*:

```
make
```

To build a deb package, [fpm](https://github.com/jordansissel/fpm) must be installed. Run the following in the directory *service*:

```
make deb
```

Binary and deb-package will appear in *build* directory.  
#### CLI arguments  

```
Usage: vspomogator [-c config] [-h] [-v]
  -c string
    	Path to config (default "vspomogator.yml")
  -h	Show this help
  -v	Show version
```

#### Configuration file  

```
port: /dev/ttyUSB0                     # The port to which the ESP32 is connected. It can be found in /dev/. It is usually /dev/ttyUSB0
layouts:
  - buttons:                           # first layout
    - label: MC                        # label on display
      type: command                    # action type (command - command, keytype - emulate keyaboard typing, keyseq - key combination. command is default)
      command:
        com: terminator                # Command to launch
        args: [-e, mc]                 # Arguments of command
    - label: Close
      type: keyseq                     # key combination
      command:
        com: "f4"                      # main key
        args: ["alt"]                  # modificators
    - label: Hello
      type: keytype                    # keyboard typing
      command:
        com: Hello, world!             # text to type
        hitenter: true                 # press enter after typing
  - buttons:                           # second layout
    ...
```

Modificators and keys for combinations look [here](https://github.com/go-vgo/robotgo/blob/master/docs/keys.md)  