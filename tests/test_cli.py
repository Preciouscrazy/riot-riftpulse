"""Cross-platform command-line regression tests for both offline RiftPulse modes."""

import json
import pathlib
import subprocess
import sys
import tempfile
import unittest

if len(sys.argv) != 2:
    raise SystemExit("Usage: test_cli.py <riftpulse-executable>")

EXECUTABLE = pathlib.Path(sys.argv[1]).resolve()
ROOT = pathlib.Path(__file__).resolve().parents[1]
MATCH = ROOT / "data" / "match_events.csv"
TELEMETRY = ROOT / "data" / "performance_samples.csv"


class CommandLineTests(unittest.TestCase):
    def execute(self, *arguments):
        return subprocess.run(
            [str(EXECUTABLE), *(str(arg) for arg in arguments)],
            cwd=ROOT, capture_output=True, text=True,
            timeout=20, check=False,
        )

    def expected(self, filename):
        return json.loads((ROOT / "evidence" / filename).read_text(encoding="utf-8-sig"))

    def test_match_output_matches_checked_in_report(self):
        result = self.execute("--input", MATCH)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout), self.expected("sample-report.json"))

    def test_telemetry_output_matches_checked_in_report(self):
        result = self.execute("--telemetry", TELEMETRY, "--stutter-ms", "33.3")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout), self.expected("telemetry-sample-report.json"))

    def test_match_file_output_and_repeated_results(self):
        with tempfile.TemporaryDirectory() as tmp:
            destination = pathlib.Path(tmp) / "match.json"
            result = self.execute("--input", MATCH, "--output", destination)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(json.loads(destination.read_text(encoding="utf-8")), self.expected("sample-report.json"))
            repeat = self.execute("--input", MATCH)
            self.assertEqual(repeat.returncode, 0, repeat.stderr)
            self.assertEqual(json.loads(repeat.stdout), json.loads(destination.read_text(encoding="utf-8")))

    def test_telemetry_file_output(self):
        with tempfile.TemporaryDirectory() as tmp:
            destination = pathlib.Path(tmp) / "telemetry.json"
            result = self.execute("--telemetry", TELEMETRY, "--output", destination)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(json.loads(destination.read_text(encoding="utf-8")), self.expected("telemetry-sample-report.json"))

    def test_invalid_usage_returns_two(self):
        for options in (
            (),
            ("--input", MATCH, "--telemetry", TELEMETRY),
            ("--input", MATCH, "--stutter-ms", "33.3"),
            ("--telemetry", TELEMETRY, "--stutter-ms", "0"),
            ("--telemetry", TELEMETRY, "--stutter-ms", "nan"),
            ("--something-else",),
        ):
            with self.subTest(options=options):
                result = self.execute(*options)
                self.assertEqual(result.returncode, 2)
                self.assertIn("Usage:", result.stderr)

    def test_missing_input_returns_one(self):
        with tempfile.TemporaryDirectory() as tmp:
            result = self.execute("--input", pathlib.Path(tmp) / "nonexistent.csv")
            self.assertEqual(result.returncode, 1)
            self.assertIn("could not open input", result.stderr)

    def test_same_input_output_does_not_overwrite_csv(self):
        for mode, original in (("--input", MATCH), ("--telemetry", TELEMETRY)):
            with self.subTest(mode=mode):
                with tempfile.TemporaryDirectory() as tmp:
                    source = pathlib.Path(tmp) / "source.csv"
                    source.write_bytes(original.read_bytes())
                    before = source.read_bytes()
                    result = self.execute(mode, source, "--output", source)
                    self.assertEqual(result.returncode, 1)
                    self.assertIn("same file", result.stderr)
                    self.assertEqual(source.read_bytes(), before)

    def test_partially_invalid_match_reports_exit_three(self):
        with tempfile.TemporaryDirectory() as tmp:
            source = pathlib.Path(tmp) / "mixed.csv"
            source.write_text(
                "match_id,timestamp_seconds,player_id,event_type,value\n"
                "M1,0,player_one,kill,\n"
                "M1,-2,player_two,death,\n", encoding="utf-8"
            )
            result = self.execute("--input", source)
            self.assertEqual(result.returncode, 3)
            self.assertEqual(json.loads(result.stdout)["invalid_rows"], 1)
            self.assertIn("Warning:", result.stderr)

    def test_partially_invalid_telemetry_reports_exit_three(self):
        with tempfile.TemporaryDirectory() as tmp:
            source = pathlib.Path(tmp) / "mixed.csv"
            source.write_text(
                "timestamp_ms,frame_time_ms,latency_ms\n"
                "10,16,\n"
                "15,-2,40\n", encoding="utf-8"
            )
            result = self.execute("--telemetry", source)
            self.assertEqual(result.returncode, 3)
            report = json.loads(result.stdout)
            self.assertEqual(report["invalid_rows"], 1)
            self.assertEqual(report["frame_time"]["sample_count"], 1)
            self.assertIn("Warning:", result.stderr)

    def test_invalid_telemetry_header_fails(self):
        with tempfile.TemporaryDirectory() as tmp:
            source = pathlib.Path(tmp) / "bad.csv"
            source.write_text("frame_time_ms\n16\n", encoding="utf-8")
            result = self.execute("--telemetry", source)
            self.assertEqual(result.returncode, 1)
            self.assertIn("Error:", result.stderr)

    def test_empty_match_csv_fails(self):
        with tempfile.TemporaryDirectory() as tmp:
            source = pathlib.Path(tmp) / "empty.csv"
            source.write_text(
                "match_id,timestamp_seconds,player_id,event_type,value\n",
                encoding="utf-8",
            )
            result = self.execute("--input", source)
            self.assertEqual(result.returncode, 1)
            self.assertIn("no valid events", result.stderr)


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]], verbosity=2)
