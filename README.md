# CrabSense Hardware Water Quality Analysis V2 — bench firmware

Targets: EITEK WERS3-8IO-4AI (master, 4 MB flash) and ESP32-S3 N16R8 (slave, 16 MB flash / 8 MB PSRAM).
Legacy root sketch and drivers remain for reference, excluded from the new build. Do NOT flash legacy firmware onto V2 wiring.

## Build / bench
```sh
pio run -e eitek-master
pio run -e esp32-slave
```
Use separate serial ports when uploading; monitor UART at 115200. Default builds inhibit GPIO outputs and RS485 transmission. Disconnect all actual loads during first boot/flashing, especially EITEK GPIO19/20 shared with native USB functionality; firmware cannot guarantee hardware reset states. Confirm relay/MOSFET polarity, boot pull states, transceiver logic, EITEK RS485 direction, ADC mode and scale, power limits and flyback/overcurrent protection before enabling real hardware. No automatic chemistry or network endpoints in this milestone.

See [bench wiring and commissioning](docs/v2-bench.md) and [register contract](docs/v2-modbus.md). CI builds both targets and runs shared host tests. Hardware acceptance is not implied by compilation.

## P0.6 USB camera commissioning
A third, **camera-only** target for the same ESP32-S3 secondary board receives native USB UVC MJPEG and exports one fresh JPEG via UART. Do not run with load power connected. See [camera bench](docs/v2-camera.md). P0.8 also integrates on-demand UVC in the dosing slave, with an exclusive disarmed capture phase. Kiosk upload remains pending. CI builds all three targets.

## P0.8 / P0.9
Both boards publish declared wiring manifests on UART at boot and via `inventory`. Slave can capture a JPEG with the same firmware that controls dosing, but USB host and load operation are mutually exclusive. See [integration and inventory](docs/v2-integration.md). No physical-load discovery, Kiosk provisioning, chemical automation or hardware timing validation is claimed.

## P1.1 dosing calibration
Seven independent pump calibrations on the slave, staged two-point model with three repeats per point and an independent interior verification. Verified data are stored per channel in NVS. `volume CH UL` requires validated calibration, no extrapolation. Raw `calpulse CH MS` is a separate opt-in commissioning operation; old `dose CH MS` removed. See [calibration](docs/v2-dosing-calibration.md). Both boards must be updated together (Modbus V3).
