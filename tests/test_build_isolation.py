import configparser
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]

class BuildIsolation(unittest.TestCase):
    def test_legacy_root_is_unconditionally_non_buildable(self):
        first = (ROOT / "CrabSense-Hardware.ino").read_text().splitlines()[0]
        self.assertTrue(first.startswith("#error "), first)

    def test_exact_v2_source_filters(self):
        cfg = configparser.ConfigParser(interpolation=None)
        cfg.read(ROOT / "platformio.ini")
        self.assertEqual(cfg["platformio"]["src_dir"].strip(), "firmware")
        self.assertEqual({x.strip() for x in cfg["platformio"]["default_envs"].split(",")},
                         {"eitek-master", "esp32-slave"})
        targets = {"eitek-master", "esp32-slave", "esp32-camera-bench"}
        self.assertEqual({x[4:] for x in cfg.sections() if x.startswith("env:")}, targets)
        for target in targets:
            self.assertEqual(cfg["env:" + target]["build_src_filter"].strip(),
                             "-<*> +<" + target + "/>")
        self.assertEqual(cfg["env:esp32-camera-bench"]["extends"], "env:esp32-slave")
        workflow = (ROOT / ".github/workflows/firmware.yml").read_text()
        self.assertIn("env: [eitek-master, esp32-slave, esp32-camera-bench]", workflow)
        self.assertNotIn("pio run -t upload", workflow)

    def test_default_control_flags_are_off(self):
        config = (ROOT / "platformio.ini").read_text()
        self.assertIsNone(re.search(r"-D\s*WQ_ENABLE_[A-Z0-9_]+(?:\s|=|$)", config))
        for path, names in {
            "firmware/shared/Hardware.h": ["OUTPUTS", "RS485"],
            "firmware/eitek-master/main.cpp": [
                "CLEAN_SUPERVISED", "BATCH_SUPERVISED", "NH3_SUPERVISED",
                "NO3_SUPERVISED", "NO2_SUPERVISED", "CAL_PULSE"],
            "firmware/esp32-slave/main.cpp": ["CAL_PULSE", "PT100"],
        }.items():
            source = (ROOT / path).read_text()
            for name in names:
                flag = "WQ_ENABLE_" + name
                self.assertRegex(source, rf"#ifndef\s+{flag}\s*\n#define\s+{flag}\s+0\s*\n#endif")

if __name__ == "__main__":
    unittest.main()
