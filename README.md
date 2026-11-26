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

## P1.2 supervised Sera NO2
Opt-in, supervised state machine: measured 5 mL sample, separately measured equivalents of five drops of each reagent, operator mixing confirmations, 300 s reaction, correlated image request/confirmation, drain. No autonomous chemistry, automatic mixing or light relay during capture. Disabled by default. See [NO2 commissioning](docs/v2-no2.md). Both boards require Modbus V4.

## P1.3 supervised VASTOCEAN NO3
Opt-in sequential assay, 5 mL, three liquid doses each equivalent to seven drops and one manual 1/3 scoop powder step with 10 s mixing gate, 120 s reaction, correlated image confirmation, OUT4/OUT7 drain. Confirm exact kit leaflet before use. NO2 and NO3 mutually exclusive. See [NO3 commissioning](docs/v2-no3.md). Modbus remains V4; update both boards to receive the slave loop timeout/receipt fix.

## P1.4 supervised Thuy Phuoc NH3/NH4
Opt-in, measured 5 mL sample, two liquid doses CH6/7 each equivalent to five drops, operator mixing, 600 s target reaction, image confirmation, OUT5/OUT7 drain. Candidate recipe based on retailer instructions, original kit leaflet and water-matrix compatibility must be verified before operation. NO2/NO3/NH3 share exclusive ownership through completion or fault until explicit reset. See [NH3 commissioning](docs/v2-nh3.md). Modbus V4 unchanged. No concentration inference.

## P1.4.1 NH3 sample evidence
At sample pulse completion before any reagent, master snapshots calibrated pH and fresh Modbus temperature observation. Missing/invalid readings are null. Emits NH3 SAMPLE JSON and NH3 IMAGE_LINK JSON with shared boot-session identifier; nh3 evidence reprints snapshot while no output active. Boot uptime only; gateway must establish real UTC/time alignment and physical sensor-vial association. No salinity inferred from TDS and no NH3 conversion on firmware. See [NH3 commissioning](docs/v2-nh3.md).

## P1.5 supervised batch coordinator
Manual mask 1..7 or scheduled request (all three). Independent reaction timers with exclusive actuator/camera ownership; longest reactions first; reject deadline conflicts, require manual routing/mixing/powder and optical positioning, verify image identity before drain; one shared total drain. Disabled by default; no RTC schedule or unattended chemistry. See [batch commissioning](docs/v2-batch.md). Modbus V4 unchanged.
