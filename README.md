# Volumetric Radar

This project aims to provide an advanced and precise object detection system using multiple custom radar devices. By combining the data from multiple radars, it generates a volumetric representation of the environment, enabling accurate detection of objects in 3D space.

The project is divided into the four following components:

    ├── client      # The web-app used to interact with either a single module or all through the bridge
    ├── firmware    # The programming run on the radar module iteself
    ├── hardware    # The pcb, enclosure, and radar module
    │   ├── pcb
    │   ├── enclosure
    │   └── radar
    └── bridge      # The software that unifies multiple modules into a single visualization

## vRadar Mk4 Hardware

The vRadar Mk4 is a k-band FMCW (Frequency-Modulated Continuous-Wave) MIMO (Multi-Input Multi-Output) IoT radar device.

![vradar-mk4-full.png](docs%2Fassets%2Fvradar-mk4-full.png)

### Features

- USB-C for power supply and debug
- Dual Core Microcontroller (ESP32-S3-WROOM-1U-N4R2)
  - 260 Mhz Clock Speed
  - 4 MB Flash
  - 2 MB PSRAM + 512 KB RAM
- LSM6DSM Gyroscope & Accelerometer
- External 2.4 Ghz Wi-Fi Antenna
  - 150 Mbps @ 20m line of sight with moderate obstructions
- Configurable audible chirp (can be used as an inordinately precise metronome)
- 24 GHz FMCW Radar Module (RF Beam K-LC7)
  - 24-24.25 GHz (&approx; 250 Mhz VCO Modulation Bandwidth)
  - 1 transmitter, 2 receivers
- MCP4922 12-bit DAC (used for Buzzer and VCO)
- 4 x 73dB dual-stage amplifier

### Runtime Abstract

### Communication

The vRadar Mk4 device communicates over Wi-Fi. The device creates an access point when first initialized, connecting to
this access point with a qualified smartphone will automatically pull up a captive portal where a network can be
selected.

Once connected, the device can be accessed through websockets.

```
Client -> [Open] ->  vRadar
vRadar -> Metadata -> Client
Client -> Ping -> vRadar

vRadar -> BINARY -> Client
vRadar -> diagnostic -> Client
vRadar -> BINARY -> Client
vRadar -> diagnostic -> Client

...

Client -> [Close] -> vRadar
```

#### Binary Data

Binary data is sent periodically depending on the sample rate and the chirp duration. With the default configuration,
each chirp will be sent as its own packet over the network.

The binary data in a manor designed to quickly get the data off of the device. As such, the data is simply four arrays
of unsigned 16-bit integers concatenated back to back. Additionally, there will be 16 bytes of timing data at the end.

The timing data is two signed 64-bit integers. The integers represent the number of microseconds since the
microcontroller booted, the first number is the start of the measured chirp, the second is the end. The difference is
the number of microseconds elapsed during a single chirp sample.

#### Diagnostic Message

```json
{
    "pitch": -2.0781369209289551,
    "roll": 25.851993560791016,
    "temperature": 63.541999816894531,
    "rssi": -21
}
```

### Configuration

This endpoint can be called at any time during the runtime. All the internal components will gracefully initialize and
reinitialize without restarting or disconnecting from the network. All mutations will be applied within 750ms on
average, depending on the current runtime state. Many of the variables have fail safes defaults to prevent the device
from crashing and entering a boot loop.

If the device does get stuck in a configuration bound boot loop, you can manually erase the flash and flash (simply
flashing won't work over the ESP32-S3's internal USB controller).

`POST /system`

```json
{
    "chirp": {
        "prf": 10000,
        "duration":10000,
        "steps": 100,
        "padding": 0,
        "resolution": 1000
    },
    "sampling": {
        "frequency": 20000,
        "samples":1,
        "attenuation": 3
    },
    "audible": 0,
    "gyro": 1,
    "enable": 1
}
```

Upon a validation the changes will be pushed to the onboard flash. Changes will not be applied immediately and can take
up to 1500ms.

An example response has been provided below:

```json
{
    "name": "vRadar df15",
    "mac": "7c:df:a1:f4:15:78",
    "base": 24125000000,
    "xFov": 80,
    "yFov": 34,
    "enabled": 1,
    "audible": 0,
    "gyro": 1,
    "sampling": {
        "frequency": 20000,
        "samples": 1,
        "attenuation": 3
    },
    "chirp": {
        "prf": 10000,
        "duration": 10000,
        "steps": 100,
        "padding": 0,
        "resolution": 1000
    }
}
```

### Hardware

#### PCB

#### Radar Module

#### Enclosure

### Firmware

### Client

### Bridge

---

**Copyright &copy; Braden Nicholson 2023**

This project is available for reference purposes only and may not be copied, distributed, or used for commercial purposes without the express permission of the author. Please respect the time and effort that went into creating this project.