import unittest
import zlib
from tools.capture_camera import Capture

class CameraTransport(unittest.TestCase):
    def setUp(self):
        self.jpg = b"\xff\xd8\x01\x02\xff\xd9"
        self.c = Capture(7)
        self.header = f"CAM BEGIN 7 10 320 240 6 {zlib.crc32(self.jpg):08x}"
    def test_complete(self):
        self.assertIsNone(self.c.feed("CAM BEGIN 8 1 1 1 99 0"))
        self.c.feed(self.header)
        self.c.feed("CAM DATA 7 0 ffd801")
        self.c.feed("CAM DATA 7 3 02ffd9")
        self.assertEqual(self.c.feed("CAM END 7"),self.jpg)
    def test_gap(self):
        self.c.feed(self.header)
        with self.assertRaises(ValueError): self.c.feed("CAM DATA 7 3 02ffd9")
    def test_corruption(self):
        self.c.feed(self.header)
        self.c.feed("CAM DATA 7 0 ffd80103ffd9")
        with self.assertRaises(ValueError): self.c.feed("CAM END 7")
    def test_timeout(self):
        with self.assertRaises(ValueError): self.c.feed("CAM ERROR 7 timeout")
    def test_oversize(self):
        with self.assertRaises(ValueError): self.c.feed("CAM BEGIN 7 1 320 240 999999 0")
    def test_truncated(self):
        self.c.feed(self.header)
        self.c.feed("CAM DATA 7 0 ffd8")
        with self.assertRaises(ValueError): self.c.feed("CAM END 7")
