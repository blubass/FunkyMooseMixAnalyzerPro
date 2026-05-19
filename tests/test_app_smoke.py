import json
import os
import sqlite3
import tempfile
import time
import unittest
from unittest import mock


TEST_DATA_DIR = tempfile.mkdtemp(prefix="mix-analyzer-test-")
os.environ["MIX_ANALYZER_DATA_DIR"] = TEST_DATA_DIR

import app as mix_app
from core.plugin_bridge import PluginBridge


def make_slice(tag="Intro", start=0.0, duration=10.0):
    return {
        "tag": tag,
        "start": start,
        "duration": duration,
        "sr": 48000,
        "rms_db": -18.0,
        "peak_db": -9.0,
        "crest_db": 9.0,
        "correlation": 0.8,
        "mid_side": {"width_pct": 20.0, "ms_ratio_db": -14.0},
        "levels": {
            "mono": {"rms_db": -20.0, "peak_db": -12.0, "crest_db": 8.0},
            "left": {"rms_db": -18.0, "peak_db": -9.0, "crest_db": 9.0},
            "right": {"rms_db": -19.0, "peak_db": -10.0, "crest_db": 9.0},
            "max_channel_peak_db": -9.0,
            "combined_rms_db": -18.5,
        },
        "I": -10.0,
        "LRA": 5.0,
        "TP": -1.0,
        "SP": -1.5,
        "loudness_method": "ffmpeg loudnorm",
        "bands": [
            {"name": "Sub", "range": "20-60Hz", "percent": 8.0},
            {"name": "Bass", "range": "60-250Hz", "percent": 14.0},
            {"name": "Low-Mids", "range": "250-500Hz", "percent": 16.0},
            {"name": "Mids", "range": "500-2000Hz", "percent": 24.0},
            {"name": "Presence", "range": "2000-6000Hz", "percent": 20.0},
            {"name": "Air", "range": "6000-20000Hz", "percent": 18.0},
        ],
        "quality": {
            "clipped_percent": 0.0,
            "silence_percent": 1.0,
            "dc_offset": 0.0,
            "stereo_balance_db": 1.0,
            "spectral_centroid_hz": 1200.0,
            "spectral_rolloff_hz": 6400.0,
        },
        "resonances": [],
        "target_curve": [],
        "waveform_url": f"/media/images/test_{tag}_waveform.png",
        "spectrum_url": f"/media/images/test_{tag}_spectrum.png",
    }


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

    def test_plugin_bridge_keeps_worst_case_safety_metrics(self):
        bridge = PluginBridge()
        args = [
            -14.0, -13.0, -12.0, -1.8, 6.0, 10.0,
            0.7, 65.0, -0.4, 0.01, 0.2,
            2500.0, 12000.0, 4200.0, 4.0,
            5.0, 8.0, 12.0, 30.0, 24.0, 8.0,
            91, "ready", "Full pass OK", 96,
            180.0, 1, 0.4,
            94, 91, 88, 85, 97,
            92, 1,
            1, 65.0, -14.0, -1.0, 2.5, -0.4, -0.8, 0.2, 92.0, 0.6, 1.1,
            -11.5, -1.0, -2.5, 2.5, 0.4, 91.0,
        ]

        bridge._handle_metrics("/fmma/metrics", *args)

        metrics = bridge.get_latest_metrics()
        self.assertEqual(metrics["safety"]["worstTruePeak"], 0.2)
        self.assertEqual(metrics["safety"]["worstClipping"], 0.4)
        self.assertEqual(metrics["assessment"]["confidenceDomains"]["tone"], 85)
        self.assertEqual(metrics["assessment"]["releaseGate"]["score"], 92)
        self.assertTrue(metrics["assessment"]["releaseGate"]["ready"])
        self.assertTrue(metrics["autoMaster"]["enabled"])
        self.assertEqual(metrics["autoMaster"]["gainDb"], 2.5)
        self.assertEqual(metrics["autoMaster"]["widthPercent"], 92.0)
        self.assertEqual(metrics["autoMaster"]["glueReductionDb"], 1.1)
        self.assertEqual(metrics["autoMaster"]["projectedLufs"], -11.5)
        self.assertEqual(metrics["autoMaster"]["projectedTruePeakDbTp"], -1.0)
        self.assertEqual(metrics["autoMaster"]["loudnessMatchGainDb"], -2.5)
        self.assertEqual(metrics["autoMaster"]["lufsDeltaDb"], 2.5)
        self.assertEqual(metrics["autoMaster"]["truePeakMarginDb"], 0.4)
        self.assertEqual(metrics["autoMaster"]["releaseScore"], 91.0)

    def test_plugin_bridge_defaults_worst_clipping_for_older_messages(self):
        bridge = PluginBridge()
        args = [
            -14.0, -13.0, -12.0, -1.8, 6.0, 10.0,
            0.7, 65.0, -0.4, 0.03, -0.9,
            2500.0, 12000.0, 4200.0, 4.0,
            5.0, 8.0, 12.0, 30.0, 24.0, 8.0,
            91, "ready", "Full pass OK", 96,
            180.0, 1,
        ]

        bridge._handle_metrics("/fmma/metrics", *args)

        metrics = bridge.get_latest_metrics()
        self.assertEqual(metrics["safety"]["worstClipping"], 0.03)
        self.assertIsNone(metrics["assessment"]["confidenceDomains"])
        self.assertIsNone(metrics["assessment"]["releaseGate"])
        self.assertIsNone(metrics["autoMaster"])

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

    def test_summary_prefers_full_track_loudness(self):
        slices = [make_slice("Intro")]

        summary = mix_app.build_summary(
            slices,
            "Pop",
            total_duration=60,
            track_loudness={"I": -10.5, "LRA": 5.0, "TP": -1.0, "method": "ffmpeg loudnorm"},
        )

        self.assertEqual(summary["measured_lufs"], -10.5)
        self.assertEqual(summary["loudness_scope"], "full-track")

    def test_slice_lufs_aggregation_is_energy_weighted(self):
        loud = make_slice("Intro", duration=10.0)
        quiet = make_slice("Outro", duration=10.0)
        loud["I"] = -10.0
        quiet["I"] = -20.0

        aggregate = mix_app.aggregate_slices([loud, quiet])

        self.assertAlmostEqual(aggregate["I"], -12.6, places=1)

    def test_summary_scores_near_miss_ranges_proportionally(self):
        slice_data = make_slice("Middle", duration=30.0)
        slice_data["I"] = -10.0
        slice_data["bands"] = [
            {"name": "Sub", "range": "20-60Hz", "percent": 3.0},
            {"name": "Bass", "range": "60-250Hz", "percent": 5.0},
            {"name": "Low-Mids", "range": "250-500Hz", "percent": 16.0},
            {"name": "Mids", "range": "500-2000Hz", "percent": 24.0},
            {"name": "Presence", "range": "2000-6000Hz", "percent": 20.0},
            {"name": "Air", "range": "6000-20000Hz", "percent": 18.0},
        ]

        summary = mix_app.build_summary([slice_data], "Pop", total_duration=30)

        self.assertEqual(summary["verdict"], "polish")
        self.assertEqual(summary["score_components"]["low_end"], 92)
        self.assertGreaterEqual(summary["overall_score"], 90)

    def test_band_distribution_uses_detailed_mix_bands(self):
        sr = 48000
        samples = mix_app.np.zeros((sr, 2), dtype=mix_app.np.float32)

        bands, *_ = mix_app.band_distribution(samples, sr, "Pop")

        self.assertEqual(
            [band["name"] for band in bands],
            ["Sub", "Bass", "Low-Mids", "Mids", "Presence", "Air"],
        )

    def test_levels_report_channel_peaks_and_mono_sum(self):
        samples = mix_app.np.array([[0.5, -0.25], [-0.5, 0.25]], dtype=mix_app.np.float32)

        rms_db, peak_db, crest_db, mid_side, details = mix_app.levels(samples)

        self.assertAlmostEqual(peak_db, -6.021, places=2)
        self.assertAlmostEqual(details["left"]["peak_db"], -6.021, places=2)
        self.assertAlmostEqual(details["right"]["peak_db"], -12.041, places=2)
        self.assertAlmostEqual(details["mono"]["peak_db"], -18.062, places=2)
        self.assertAlmostEqual(rms_db, details["combined_rms_db"], places=3)
        self.assertGreater(crest_db, 0)
        self.assertGreater(mid_side["width_pct"], 0)

    def test_slice_endpoint_replaces_existing_tag_and_rejects_unknown(self):
        client = mix_app.app.test_client()
        req_id = "dup-slice-test"
        save_filename = "upload_dup_slice_test.wav"
        save_path = os.path.join(mix_app.app.config["UPLOAD_FOLDER"], save_filename)
        with open(save_path, "wb") as handle:
            handle.write(b"placeholder")

        initial_data = {
            "id": req_id,
            "filename": "dup.wav",
            "genre": "Pop",
            "total_duration": 10.0,
            "slices": [],
            "slices_meta": mix_app.build_slices_meta(10.0),
            "track_loudness": {"I": -10.0, "LRA": 5.0, "TP": -1.0, "method": "ffmpeg loudnorm"},
            "audio_url": f"/media/audio/{save_filename}",
        }
        with sqlite3.connect(mix_app.DB_PATH) as conn:
            conn.execute("DELETE FROM analyses WHERE id = ?", (req_id,))
            conn.execute(
                "INSERT INTO analyses (id, filename, genre, data) VALUES (?, ?, ?, ?)",
                (req_id, "dup.wav", "Pop", json.dumps(initial_data)),
            )

        def fake_analyze(_ffmpeg_cmd, _src, start, duration, tag, _req_id, _genre, _snippet_dir, _image_dir):
            return make_slice(tag, start, duration)

        with mock.patch.object(mix_app, "analyze_slice", side_effect=fake_analyze):
            first = client.post(f"/analyze_slice/{req_id}/Intro")
            second = client.post(f"/analyze_slice/{req_id}/Intro")
            invalid = client.post(f"/analyze_slice/{req_id}/Bridge")

        self.assertEqual(first.status_code, 200)
        self.assertEqual(second.status_code, 200)
        self.assertEqual(invalid.status_code, 400)
        payload = second.get_json()["current_analysis"]
        self.assertEqual([slice_data["tag"] for slice_data in payload["slices"]], ["Intro"])
        self.assertTrue(second.get_json()["is_complete"])

    def test_cleanup_removes_stale_non_references_and_keeps_references(self):
        old_id = "old-cleanup-test"
        ref_id = "ref-cleanup-test"
        old_audio = "upload_old_cleanup.wav"
        old_waveform = "old_cleanup_waveform.png"
        old_spectrum = "old_cleanup_spectrum.png"
        ref_audio = "upload_ref_cleanup.wav"
        ref_waveform = "ref_cleanup_waveform.png"
        ref_spectrum = "ref_cleanup_spectrum.png"

        for directory, filename in [
            (mix_app.app.config["UPLOAD_FOLDER"], old_audio),
            (mix_app.app.config["IMAGE_FOLDER"], old_waveform),
            (mix_app.app.config["IMAGE_FOLDER"], old_spectrum),
            (mix_app.app.config["UPLOAD_FOLDER"], ref_audio),
            (mix_app.app.config["IMAGE_FOLDER"], ref_waveform),
            (mix_app.app.config["IMAGE_FOLDER"], ref_spectrum),
        ]:
            path = os.path.join(directory, filename)
            with open(path, "wb") as handle:
                handle.write(b"media")
            old_time = time.time() - mix_app.MEDIA_TTL_SECONDS - 60
            os.utime(path, (old_time, old_time))

        old_data = {
            "audio_url": f"/media/audio/{old_audio}",
            "slices": [{"waveform_url": f"/media/images/{old_waveform}", "spectrum_url": f"/media/images/{old_spectrum}"}],
        }
        ref_data = {
            "audio_url": f"/media/audio/{ref_audio}",
            "slices": [{"waveform_url": f"/media/images/{ref_waveform}", "spectrum_url": f"/media/images/{ref_spectrum}"}],
        }
        with sqlite3.connect(mix_app.DB_PATH) as conn:
            conn.execute("DELETE FROM analyses WHERE id IN (?, ?)", (old_id, ref_id))
            conn.execute(
                "INSERT INTO analyses (id, filename, genre, timestamp, is_reference, data) "
                "VALUES (?, ?, ?, datetime('now', '-7 hours'), 0, ?)",
                (old_id, "old.wav", "Pop", json.dumps(old_data)),
            )
            conn.execute(
                "INSERT INTO analyses (id, filename, genre, timestamp, is_reference, data) "
                "VALUES (?, ?, ?, datetime('now', '-7 hours'), 1, ?)",
                (ref_id, "ref.wav", "Pop", json.dumps(ref_data)),
            )

        mix_app.cleanup_old_files()

        with sqlite3.connect(mix_app.DB_PATH) as conn:
            remaining_ids = {row[0] for row in conn.execute("SELECT id FROM analyses WHERE id IN (?, ?)", (old_id, ref_id))}

        self.assertNotIn(old_id, remaining_ids)
        self.assertIn(ref_id, remaining_ids)
        self.assertFalse(os.path.exists(os.path.join(mix_app.app.config["UPLOAD_FOLDER"], old_audio)))
        self.assertFalse(os.path.exists(os.path.join(mix_app.app.config["IMAGE_FOLDER"], old_waveform)))
        self.assertTrue(os.path.exists(os.path.join(mix_app.app.config["UPLOAD_FOLDER"], ref_audio)))
        self.assertTrue(os.path.exists(os.path.join(mix_app.app.config["IMAGE_FOLDER"], ref_waveform)))


if __name__ == "__main__":
    unittest.main()
