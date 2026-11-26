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
