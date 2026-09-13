"""Offline checks: never contacts ElevenLabs or reads a real API key."""
import io
import json
import unittest
from unittest.mock import patch
import urllib.error

import generate_audio as audio


class GenerationTests(unittest.TestCase):
    def setUp(self):
        self.config = audio.load_assets()

    def test_manifest_and_prompts(self):
        self.assertEqual(len(self.config["assets"]), 42)
        self.assertTrue(all(a["resolved_prompt"] for a in self.config["assets"]))
        self.assertEqual(len(self.config["samples"]), 4)
        for asset in self.config["assets"]:
            limit = 4100 if asset["kind"] == "music" else 450
            self.assertLessEqual(len(asset["resolved_prompt"]), limit)

    def test_shared_style_comes_from_document(self):
        self.assertEqual(self.config["prompt_influence"], 0.4)
        forbidden = ("sharp", "piercing", "crisp glass", "bright crack",
                     "violent electrical", "glass shatter", "brittle")
        for asset in self.config["assets"]:
            self.assertNotIn("prompt", asset)
            lowered = asset["resolved_prompt"].lower()
            for phrase in forbidden:
                self.assertNotIn(phrase, lowered, asset["name"])

    def test_samples_duration_floor(self):
        samples = [a for a in self.config["assets"] if a["name"] in self.config["samples"]]
        durations = [audio.request_body(a, self.config)[1]["duration_seconds"] for a in samples]
        self.assertEqual(durations, [1.6, 5.0, 3.5, 2.2])

    def test_music_instrumental_and_loop_sfx(self):
        music = next(a for a in self.config["assets"] if a["kind"] == "music")
        endpoint, body = audio.request_body(music, self.config)
        self.assertEqual(endpoint, "/v1/music")
        self.assertTrue(body["force_instrumental"])
        self.assertEqual(body["music_length_ms"], 75000)
        loop = next(a for a in self.config["assets"] if a["name"] == "SFX_Laser_Loop")
        self.assertTrue(audio.request_body(loop, self.config)[1]["loop"])

    def test_http_error_does_not_leak_secret_or_retry(self):
        fake_key = "FAKE_TEST_CREDENTIAL"
        error = urllib.error.HTTPError("https://api.elevenlabs.io", 401, "Unauthorized", {},
            io.BytesIO(json.dumps({"detail": {"status": "invalid_api_key", "message": fake_key}}).encode()))
        with patch.object(audio.urllib.request, "build_opener") as opener:
            opener.return_value.open.side_effect = error
            with self.assertRaises(RuntimeError) as caught:
                audio.generate_request("/v1/sound-generation", {}, fake_key)
            self.assertNotIn(fake_key, str(caught.exception))
            self.assertIn("invalid_api_key", str(caught.exception))
            self.assertEqual(opener.return_value.open.call_count, 1)

    def test_default_preview_no_network(self):
        with patch("sys.argv", ["generate_audio.py"]), patch("sys.stdout", new_callable=io.StringIO) as output:
            with patch.object(audio, "generate_request") as request:
                audio.main()
                request.assert_not_called()
            self.assertIn("Preview only", output.getvalue())

    def test_single_asset_preview(self):
        with patch("sys.argv", ["generate_audio.py", "--asset", "SFX_WeakPoint_Hit"]):
            with patch("sys.stdout", new_callable=io.StringIO) as output:
                audio.main()
            self.assertIn("Selected 1 candidates", output.getvalue())
            self.assertNotIn("SFX_Player_Dash", output.getvalue())

    def test_first_batch_can_skip_approved_samples(self):
        argv = ["generate_audio.py", "--batch", "first", "--exclude-samples"]
        with patch("sys.argv", argv), patch("sys.stdout", new_callable=io.StringIO) as output:
            audio.main()
        self.assertIn("Selected 22 candidates", output.getvalue())
        self.assertNotIn("SFX_Boss_Takeoff:", output.getvalue())
        self.assertNotIn("SFX_Boss_Roar:", output.getvalue())
        self.assertNotIn("SFX_WeakPoint_Hit:", output.getvalue())


if __name__ == "__main__":
    unittest.main()
