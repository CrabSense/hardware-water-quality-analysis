# V2.1 wiring / P0 bench commissioning

EITEK PDF p.2–3: 24 V supply, ADS1115, isolated RS485, shared relay COM. Verify DC load ratings, common terminal and relay activation level before connecting loads.

| EITEK relay | GPIO | Load |
|---|---:|---|
| OUT1 | 20 | source pump |
| OUT2 | 19 | sample pump |
| OUT3 | 21 | NO2 drain |
| OUT4 | 42 | NO3 drain |
| OUT5 | 41 | NH3 drain |
| OUT6 | 40 | reserve vial (blocked in bench) |
| OUT7 | 39 | total drain |
| OUT8 | 38 | light |

ADS1115 SDA9/SCL8, ADC1 pH, ADC2 TDS via separate DFR0504 isolators. ADC3/4 reserved. DI unused. W5500 CS10/MOSI11/MISO13/SCK12/RST14 reserved. RS485 TX17/RX18; direction not stated in PDF. ADS address and analog scaling require measurement; pin voltage is not terminal voltage. Configure 0–10 V mode. Never short isolation barrier.

Slave: dosing GPIO4,5 (NO2); 6,7,15 (NO3); 16,17 (NH3); 18 reserve (blocked). SPI CS10/MOSI11/SCK12/MISO13; UART TX8/RX9; proposed DIR21. USB D-19/D+20 reserved for future UVC. Do not connect RO 5 V to ESP32. A/B twisted pair; confirm polarity, termination and module direction. Camera needs verified host VBUS and UVC Full-Speed; separate P0.6 camera-only firmware is described in [v2-camera.md](v2-camera.md). The dosing slave itself does not yet run UVC.

## Enable gates only on isolated test bench
Copy per-environment build flags locally; do not enable by default in tracked config.
- WQ_ENABLE_OUTPUTS=1, WQ_ACTIVE_LEVEL=HIGH or LOW after measurement of each board, GPIO reset behaviour and dry-contact test.
- WQ_ENABLE_RS485=1, WQ_RS485_DIR=-1 only for verified auto-direction. On slave use 21 if wired DE+/RE; EITEK direction needs vendor confirmation. Explicitly define correct direction per environment.
- WQ_ENABLE_PT100=1 after reference resistor and probe wire configuration; WQ_PT100_WIRES=2/3/4; WQ_PT100_RREF=measured resistance (430 nominal only for appropriate PT100 module).
- Master WQ_ADS_ADDRESS=verified I2C address (default probe 0x48). WQ_ADC1_SCALE / WQ_ADC2_SCALE from measured terminal-to-ADC gain; WQ_PH_SLOPE and OFFSET from end-to-end buffer calibration; WQ_TDS_FACTOR from standard and actual isolation gain. Default uncalibrated. No water treatment decisions from bench readings.
- Use regulated 5 V for DFR0504, verify supply capacity and pH/TDS noise; do not use SEN0244 beyond 1000 ppm.

## Test
First run default builds with disconnected loads; status only. Verify RS485 in enabled read-only mode and PT100 with independent thermometer. Then enable outputs only with current-limited dummy loads and confirmed hardware protection.
Master serial: status, arm, pulse 1 250, dose 1 250, stop. ARM expires 30 s; maximum pulse 10 s, one load at a time per controller; master blocks local and remote overlap. No OUT6 or CH8. Slave stops/disarms if no valid master communication for 1.5 s. STOP is best effort on link failure; disconnect power physically if needed. Serial commands are local test interfaces, not authenticated production control. Do not run unattended or use with actual reagents yet. No verified sample volume, kit timing, rinse, camera capture, Kiosk/BE integration, hardware E-stop or reliable physical interlocks in this milestone.

## Follow-up
P0 hardware checks, then calibration of seven dosing pumps and water volumes, nonblocking assay recipes based on exact kits (NO2 Sera, NO3 VASTOCEAN, NH3/NH4 Thuy Phuoc), UVC capture, Kiosk authenticated gateway integration. Run scheduled three assays only after supply and hydraulic validation.
