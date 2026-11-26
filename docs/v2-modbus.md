# Bench Modbus RTU V1

Master EITEK; slave address 1; 9600 8N1; minimum frame gap ~4.1 ms. CRC16 Modbus little-endian; words big-endian. Single master, no broadcasts, no retries of uncertain dosing commands.

FC03 read address 0, quantity 4:
0 state (0 disarmed, 1 armed idle, 2 active);
1 active channel (0 idle);
2 signed PT100 centi-C, 0x8000 invalid;
3 last accepted sequence (0 initial).

FC16 write address 16, quantity 3, byte count 6:
16 sequence, 17 command/channel, 18 duration ms.
Channel 0 + duration 0 = disarm/stop; 65535 + duration 0 = arm; 1..7 + 1..10000 ms = timed dose. Same most recent sequence/payload acknowledges without restarting. Conflicting reuse rejected (STOP always wins). Bench protocol not durable across reboot, no automatic retries, no queued operations or authenticated remote interface. Read status to maintain link, 250 ms target poll; link timeout 1500 ms disarms slave. Acknowledgment is not confirmation of physical pump motion or delivered volume. Invalid request exception, no actuation.
