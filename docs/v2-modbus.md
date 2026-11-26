# Bench Modbus RTU V3 (P1.1)

Master EITEK, slave address 1, 9600 8N1, CRC16 Modbus little-endian; registers big-endian. Update both controllers together. Old V2 status/command interfaces are not compatible; V3 master refuses a status without version 3. No broadcasts or automatic retries of uncertain commands.

FC03 read address 0, quantity 6:
0 state (0 disarmed, 1 armed idle, 2 active, 3 camera busy, 4 camera fault / reboot required);
1 active channel (0 idle);
2 signed PT100 centi-C, 0x8000 invalid;
3 last accepted sequence;
4 verified calibration mask bits 0..6 = CH1..7;
5 protocol version = 3.

FC16 write address 16, quantity 3: 16 sequence, 17 command, 18 argument.
0/0 STOP (disarm/cancel camera); 65535/0 ARM; 65534/0 CAPTURE only disarmed. Commands 1..7 = raw calibration pulse in ms (250..10000), require explicit WQ_ENABLE_CAL_PULSE=1 on both boards. Commands 257..263 = calibrated volume in integer microlitres (1..65535), slave checks verified model and measured interval. Both require armed idle, valid link and camera idle. Pulse must fit remaining ARM lease. CH8 blocked. No volume splitting/extrapolation. Master mask is advisory; slave independently enforces. Acceptance is not proof of completed pulse or volume delivered. Duplicate latest sequence/payload acknowledges without execution; next sequence required otherwise, STOP always wins. Status heartbeat ~250 ms, link timeout 1500 ms. No durable sequence across reboot; trusted local bench only. Camera JPEG on slave UART, capture ID = sequence or 65536 at wrap.
