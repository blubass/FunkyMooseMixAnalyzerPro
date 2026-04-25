import os
import tempfile
import unittest


TEST_DATA_DIR = tempfile.mkdtemp(prefix="mix-analyzer-test-")
os.environ["MIX_ANALYZER_DATA_DIR"] = TEST_DATA_DIR

import app as mix_app


class AppSmokeTest(unittest.TestCase):
    def test_health_endpoint_returns_json(self):
        client = mix_app.app.test_client()
        response = client.get("/health")

        self.assertEqual(response.status_code, 200)
        payload = response.get_json()
        self.assertEqual(payload["status"], "ok")
        self.assertIn("ffmpeg", payload)
        self.assertEqual(payload["data_dir"], TEST_DATA_DIR)

    def test_genres_endpoint_returns_profiles(self):
        client = mix_app.app.test_client()
        response = client.get("/genres")

        self.assertEqual(response.status_code, 200)
        payload = response.get_json()
        names = {genre["name"] for genre in payload["genres"]}
        self.assertIn("Drum & Bass", names)
        self.assertIn("Broadcast / TV", names)
        self.assertGreaterEqual(len(names), 30)

    def test_audio_extension_allowlist(self):
        self.assertTrue(mix_app.allowed_audio_file("mix.wav"))
        self.assertTrue(mix_app.allowed_audio_file("master.AIFF"))
        self.assertFalse(mix_app.allowed_audio_file("notes.txt"))

    def test_genre_loudness_targets(self):
        self.assertEqual(mix_app.get_target_lufs("EDM / Electronic"), -8)
        self.assertEqual(mix_app.get_target_lufs("Drum & Bass"), -7)
        self.assertEqual(mix_app.get_target_lufs("Broadcast / TV"), -23)
        self.assertEqual(mix_app.get_target_lufs("Podcast / Spoken Word"), -16)
        self.assertEqual(mix_app.get_target_lufs("Unknown"), -14)

    def test_summary_uses_profile_ranges_and_confidence(self):
        slices = [{
            "tag": "Middle",
            "I": -7.5,
            "LRA": 4.0,
            "crest_db": 7.0,
            "correlation": 0.7,
            "bands": [
                {"name": "Sub", "percent": 15},
                {"name": "Bass", "percent": 18},
                {"name": "Presence", "percent": 20},
            ],
            "quality": {"clipped_percent": 0.0, "silence_percent": 2.0},
            "loudness_method": "ffmpeg loudnorm",
        }]

        summary = mix_app.build_summary(slices, "Drum & Bass", total_duration=30)

        self.assertEqual(summary["target_lufs"], -7)
        self.assertEqual(summary["verdict"], "ready")
        self.assertEqual(summary["confidence"]["label"], "high")


if __name__ == "__main__":
    unittest.main()
