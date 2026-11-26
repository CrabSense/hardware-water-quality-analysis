"""Capture one fresh JPEG over the camera-only firmware's UART."""
import argparse
import secrets
import time
import zlib
from pathlib import Path

MAX_BYTES = 55 * 1024

class Capture:
    def __init__(self, request_id):
        self.id = str(request_id)
        self.meta = None
        self.data = bytearray()

    def feed(self, line):
        p = line.strip().split()
        if len(p) < 3 or p[0] != "CAM" or p[2] != self.id:
            return None
        kind = p[1]
        if kind == "ERROR":
            raise ValueError(" ".join(p[3:]))
        if kind == "BEGIN":
            if len(p) != 8 or self.meta is not None:
                raise ValueError("unexpected frame header")
            seq, w, h, size = map(int, p[3:7])
            crc = int(p[7], 16)
            if not (0 <= seq <= 0xffffffff and 0 < w <= 65535 and 0 < h <= 65535 and 4 <= size <= MAX_BYTES and 0 <= crc <= 0xffffffff):
                raise ValueError("invalid frame metadata")
            self.meta = (seq, w, h, size, crc)
        elif kind == "DATA":
            if len(p) != 5 or self.meta is None or int(p[3]) != len(self.data):
                raise ValueError("missing or out-of-order chunk")
            if len(p[4]) > 192:
                raise ValueError("oversize chunk")
            chunk = bytes.fromhex(p[4])
            if not chunk or len(self.data) + len(chunk) > self.meta[3]:
                raise ValueError("invalid chunk length")
            self.data.extend(chunk)
        elif kind == "END":
            if len(p) != 3 or self.meta is None:
                raise ValueError("missing frame header")
            if len(self.data) != self.meta[3] or zlib.crc32(self.data) != self.meta[4]:
                raise ValueError("JPEG length or CRC mismatch")
            if not self.data.startswith(b"\xff\xd8") or not self.data.endswith(b"\xff\xd9"):
                raise ValueError("invalid JPEG envelope")
            return bytes(self.data)
        return None

def main():
    import serial
    from PIL import Image
    from io import BytesIO
    p = argparse.ArgumentParser()
    p.add_argument("--port", required=True)
    p.add_argument("--out", required=True, type=Path)
    args = p.parse_args()
    request = secrets.randbelow(2**31-1)+1
    receiver = Capture(request)
    with serial.Serial(args.port, 115200, timeout=0.5, write_timeout=2) as port:
        # Opening a USB-UART adapter can reset the board. Wait for enumeration.
        time.sleep(3)
        port.reset_input_buffer()
        port.write(f"capture {request}\n".encode("ascii"))
        deadline = time.monotonic() + 30
        while time.monotonic() < deadline:
            line = port.readline(512).decode("ascii", errors="replace")
            data = receiver.feed(line)
            if data is not None:
                # Envelope + transport integrity alone is not proof of decodability.
                with Image.open(BytesIO(data)) as img:
                    img.load()
                    if img.format != "JPEG" or img.size != receiver.meta[1:3]:
                        raise ValueError("JPEG decode or dimension mismatch")
                args.out.write_bytes(data)
                print(f"Saved {args.out}: {receiver.meta[1]}x{receiver.meta[2]}, {len(data)} bytes")
                return
    raise TimeoutError("No complete camera frame; check UVC mode, VBUS and UART")

if __name__ == "__main__":
    main()
