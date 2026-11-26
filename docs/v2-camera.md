# P0.6 — USB UVC camera commissioning

Camera-only firmware on the ESP32-S3 N16R8 board. This is an alternative image, not a third MCU. It does not run the dosing slave, does not drive any GPIO loads, and does not connect to the network. Physically isolate all pump/valve load power before flashing; GPIO reset states are not guaranteed. Use the board's USB-UART port for upload/logging, not the native USB camera port (GPIO19/20). Verify VBUS 5 V, adequate current and no backfeed.

## Build
```sh
pio run -e esp32-camera-bench
pio run -e esp32-camera-bench -t upload --upload-port COM5
pio device monitor -p COM5 -b 115200
```
Replace COM5 with the actual UART port. Connect HUAQUE via native USB OTG. Send status; expected connected=1, valid increasing. This proves valid JPEG envelope receipt, not calibration. Driver is Espressif ESP32_USB_Stream pinned to commit 8854d03856afe1292e72d4fbb2bfdbc1ff21f61f (https://github.com/esp-arduino-libs/ESP32_USB_Stream). ESP32-S3 Full-Speed camera must advertise compatible MJPEG mode; default request 320x240 at 15 FPS, configurable WQ_CAMERA_WIDTH/HEIGHT/FPS. Frame limit 55 KiB. No exact camera descriptor or fixed exposure/white balance verified.

## Save JPEG
Close Serial Monitor first.
```sh
python -m pip install pyserial pillow
python tools/capture_camera.py --port COM5 --out sample.jpg
```
The tool requests a fresh frame using a random ID, checks frame size, offsets, CRC32, JPEG envelope and decodes with Pillow before saving. A missing/invalid frame fails rather than fabricating a result. If camera enumeration is slow, wait until status shows connected then retry. Test unplug/replug, disconnected capture, ten captures, verify all JPEGs can be opened. Transport is a local diagnostic via UART, not a production image upload protocol. No chemical concentration inferred.

## Gate to integration
Verify actual HUAQUE USB VID/PID, descriptor, resolution, bandwidth, stable frames, available RAM, lighting/locked exposure and white balance. Only then integrate UVC in dosing slave with independent execution and verified OFF deadlines under USB load. Define capture command, image upload to Kiosk and failure policy. Reflash esp32-slave after camera-only test; never enable loads in camera test. No hardware verification claimed by CI.
