#!/usr/bin/env python3
"""Generate RDCA audition candidates using ElevenLabs (Python standard library).

No network requests without --generate. No automatic paid retries. Credentials
are accepted only via environment, hidden prompt or stdin, never CLI arguments.
"""
from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import urllib.error
import urllib.request
import wave

ROOT = Path(__file__).resolve().parents[1]
DOC = ROOT / "docs/audio_asset_requirements.md"
MANIFEST = ROOT / "tools/audio_manifest.json"
OUTPUT = ROOT / "audio/generated"
SFX_COMMON = (
    "Layered game SFX with believable physics, full body, controlled highs and "
    "natural decay. No music, dialogue, human voice or clipping."
)


def load_assets():
    config = json.loads(MANIFEST.read_text(encoding="utf-8"))
    document = DOC.read_text(encoding="utf-8-sig")
    # Keep the complete source prompt in the requirements document authoritative.
    sections = re.split(r"(?m)(?=^#{3,4} `)", document)
    prompts = {}
    for section in sections:
        heading = re.match(r"^#{3,4} `([^`]+)`", section)
        prompt = re.search(r"```text\s*\n(.*?)```", section, re.S)
        if heading and prompt:
            prompts[heading.group(1)] = " ".join(prompt.group(1).split())
    seen = set()
    for asset in config["assets"]:
        name = asset["name"]
        if not re.fullmatch(r"(?:SFX|BGM|AMB)_[A-Za-z0-9_]+", name) or name in seen:
            raise ValueError("Invalid or duplicate asset name: " + name)
        seen.add(name)
        prompt = asset.get("prompt") or prompts.get(asset.get("prompt_from", name))
        if not prompt:
            raise ValueError("No source prompt for " + name)
        if asset.get("variation"):
            prompt += " Variation: " + asset["variation"]
        if asset["kind"] == "sfx" and name not in ("SFX_Victory", "SFX_Defeat"):
            prompt = SFX_COMMON + " " + prompt
        asset["resolved_prompt"] = prompt
        limit = 4100 if asset["kind"] == "music" else 450
        if len(prompt) > limit:
            raise ValueError(f"Prompt exceeds {limit} characters: {name}. Shorten it before generating.")
        if asset["duration"] <= 0 or asset["channels"] not in (1, 2):
            raise ValueError("Invalid duration/channels: " + name)
        if asset["kind"] == "sfx" and asset["duration"] > 30:
            raise ValueError("SFX exceeds API duration limit: " + name)
    return config


def request_body(asset, config):
    if asset["kind"] == "music":
        return "/v1/music", {
            "prompt": asset["resolved_prompt"],
            "music_length_ms": round(asset["duration"] * 1000),
            "model_id": config["music_model"],
            "force_instrumental": True,
        }
    return "/v1/sound-generation", {
        "text": asset["resolved_prompt"],
        "duration_seconds": max(0.5, asset["duration"]),
        "model_id": config["sfx_model"],
        "prompt_influence": asset.get("prompt_influence", config["prompt_influence"]),
        "loop": asset.get("loop", False),
    }


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        # Never forward an API key to a redirect destination.
        return None


def generate_request(path, body, key):
    request = urllib.request.Request(
        "https://api.elevenlabs.io" + path + "?output_format=mp3_44100_128",
        data=json.dumps(body).encode("utf-8"),
        headers={"xi-api-key": key, "Content-Type": "application/json"},
        method="POST",
    )
    try:
        with urllib.request.build_opener(NoRedirect).open(request, timeout=180) as response:
            data = response.read(30 * 1024 * 1024)
            content_type = response.headers.get("Content-Type", "")
            if len(data) < 128 or not (data.startswith(b"ID3") or data[0] == 0xFF):
                raise RuntimeError("Response is not a recognizable MP3; no automatic retry.")
            return data, {
                "content_type": content_type,
                "request_id": response.headers.get("request-id"),
                "character_cost": response.headers.get("character-cost"),
            }
    except urllib.error.HTTPError as error:
        # Do not print response text, request headers, key, or account details.
        code = "unknown"
        try:
            parsed = json.loads(error.read(8192))
            detail = parsed.get("detail", {})
            if isinstance(detail, dict):
                candidate = str(detail.get("status", "unknown"))
                if re.fullmatch(r"[a-z_]{1,64}", candidate):
                    code = candidate
        except (ValueError, AttributeError):
            pass
        raise RuntimeError(
            f"ElevenLabs HTTP {error.code} ({code}). Stopped without retry. "
            "Check key, Sound Effects/Music permissions, balance and plan."
        ) from None
    except (urllib.error.URLError, TimeoutError, OSError):
        raise RuntimeError(
            "Network/TLS/timeout error. Stopped without retry; the remote request "
            "may already have been billed. Check provider history before repeating."
        ) from None


def write_json(path, value):
    with path.open("x", encoding="utf-8") as handle:
        json.dump(value, handle, ensure_ascii=False, indent=2)
        handle.write("\n")


def convert_audio(source, asset, ffmpeg):
    target = source.parent / (asset["name"] + ".wav")
    if target.exists():
        return {"wav": target.name, "status": "already_exists"}
    args = [ffmpeg, "-nostdin", "-hide_banner", "-loglevel", "error", "-n", "-i", str(source)]
    # Preserve the complete generated performance. Signature effects need their
    # physical preparation, evolving body and decay; editing happens after audition.
    args += ["-ar", "48000", "-ac", str(asset["channels"]), "-c:a", "pcm_s16le", str(target)]
    subprocess.run(args, check=True, capture_output=True, timeout=60)
    with wave.open(str(target), "rb") as wav:
        frames = wav.getnframes()
        duration = frames / wav.getframerate()
        raw = wav.readframes(frames)
        if duration <= 0 or not any(raw):
            raise RuntimeError("Converted WAV is empty or silent: " + asset["name"])
        return {
            "wav": target.name, "sample_rate": wav.getframerate(),
            "channels": wav.getnchannels(), "bits": wav.getsampwidth() * 8,
            "duration_seconds": duration,
            "source_lossy": True,
            "trimmed": False,
            "audition_required": True,
        }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--batch", choices=["samples", "first", "music", "anchors", "details", "all"], default="samples")
    parser.add_argument("--generate", action="store_true", help="Make paid API calls (default: preview only)")
    parser.add_argument("--asset", action="append", help="Select an exact asset instead of a batch; repeatable")
    parser.add_argument("--exclude-samples", action="store_true",
                        help="Skip assets listed in manifest samples (use after sample approval)")
    parser.add_argument("--key-stdin", action="store_true", help="Read key from redirected stdin; do not use in shell history")
    parser.add_argument("--prompt-key", action="store_true", help="Enter key with getpass (hidden)")
    parser.add_argument("--ffmpeg", help="Optional ffmpeg executable for WAV conversion")
    parser.add_argument("--convert-only", type=Path, help="Convert saved originals in an existing run; no API call")
    args = parser.parse_args()
    config = load_assets()
    selected = [a for a in config["assets"] if args.batch == "all" or a["batch"] == args.batch
                or args.batch == "samples" and a["name"] in config["samples"]]
    if args.asset:
        known = {a["name"] for a in config["assets"]}
        if set(args.asset) - known:
            raise ValueError("Unknown asset name")
        selected = [a for a in config["assets"] if a["name"] in args.asset]
    if args.exclude_samples:
        selected = [a for a in selected if a["name"] not in config["samples"]]
    for asset in selected:
        path, body = request_body(asset, config)
        print(f"{asset['name']}: target={asset['duration']}s, "
              f"request={body.get('duration_seconds', asset['duration'])}s, "
              f"loop={asset.get('loop', False)}", flush=True)
    print(f"Selected {len(selected)} candidates, one request per asset.", flush=True)
    ffmpeg = args.ffmpeg or shutil.which("ffmpeg")
    if ffmpeg and not Path(ffmpeg).is_file():
        raise ValueError("ffmpeg executable does not exist")
    if args.convert_only:
        if args.generate:
            raise ValueError("--convert-only and --generate cannot be combined")
        if not ffmpeg:
            raise ValueError("--convert-only needs ffmpeg")
        folder = args.convert_only.resolve()
        if OUTPUT.resolve() not in folder.parents:
            raise ValueError("Convert only a run inside audio/generated")
        result = []
        for asset in selected:
            source = folder / (asset["name"] + ".original.mp3")
            if source.is_file():
                result.append({"name": asset["name"], **convert_audio(source, asset, ffmpeg)})
        write_json(folder / ("conversion_" + dt.datetime.now().strftime("%Y%m%d_%H%M%S_%f") + ".json"), result)
        print(f"Converted/checked {len(result)} files. No API calls.")
        return
    if not args.generate:
        print("Preview only: no API key read, no audio generated, no credits spent.")
        return
    key = os.environ.get("ELEVENLABS_API_KEY", "").strip()
    if args.key_stdin:
        key = sys.stdin.readline().strip()
    elif args.prompt_key:
        import getpass
        key = getpass.getpass("ElevenLabs API key (hidden): ").strip()
    if not key:
        raise ValueError("Set ELEVENLABS_API_KEY or use --prompt-key; never put a key in source/docs.")
    run = OUTPUT / dt.datetime.now(dt.timezone.utc).strftime("%Y%m%dT%H%M%S_%fZ")
    run.mkdir(parents=True, exist_ok=False)
    print("Run directory: " + str(run), flush=True)
    for asset in selected:
        endpoint, body = request_body(asset, config)
        print("Generating " + asset["name"], flush=True)
        # Write an intent record first. It distinguishes attempted/uncertain calls
        # after interruption from successful downloads without blindly retrying.
        metadata = {
            "provider": "ElevenLabs", "asset": asset["name"],
            "utc": dt.datetime.now(dt.timezone.utc).isoformat(),
            "endpoint": endpoint, "request": body,
            "target_seconds": asset["duration"],
            "source_format": "mp3_44100_128", "audition_required": True,
            "license": "Check account plan and ElevenLabs terms before release.",
        }
        write_json(run / (asset["name"] + ".attempt.json"), metadata)
        try:
            data, headers = generate_request(endpoint, body, key)
        except RuntimeError as error:
            write_json(run / (asset["name"] + ".error.json"), {
                "asset": asset["name"], "error": str(error), "automatic_retry": False,
            })
            raise
        source = run / (asset["name"] + ".original.mp3")
        with source.open("xb") as handle:
            handle.write(data)
        metadata.update(headers)
        metadata.update({"sha256": hashlib.sha256(data).hexdigest(), "bytes": len(data), "original": source.name})
        write_json(run / (asset["name"] + ".result.json"), metadata)
        if ffmpeg:
            converted = convert_audio(source, asset, ffmpeg)
            write_json(run / (asset["name"] + ".wav-info.json"), converted)
        print("Saved " + source.name, flush=True)
    print("Generation finished. Listen before accepting/importing candidates.", flush=True)


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, ValueError, OSError, subprocess.SubprocessError) as error:
        # The error message must never include headers, a credential, or a body.
        print("ERROR: " + str(error), file=sys.stderr)
        sys.exit(1)
