#!/usr/bin/env python3
"""Parse RDCA Unreal logs into a compact Markdown combat test report."""

from __future__ import annotations

import argparse
import re
from pathlib import Path


PATTERNS = {
    "crashes": re.compile(r"Crash impact\."),
    "attachments": re.compile(r"Phase crash state: .* -> Attached"),
    "face_transitions": re.compile(r"Attach box face transition completed\."),
    "body_rebounds": re.compile(r"Crash rebound\."),
    "weakpoint_effective": re.compile(r"Boss weak point crash\..*Effective=true"),
    "weakpoint_protected": re.compile(
        r"Boss weak point crash\..*Effective=false.*Exposed=false"
    ),
    "player_damage": re.compile(r"Player damaged\."),
    "player_defeated": re.compile(r"Player defeated\."),
    "boss_dead_state": re.compile(r"Boss encounter state\..*To=5"),
}


def parse_log(text: str) -> dict[str, int]:
    return {
        name: sum(1 for line in text.splitlines() if pattern.search(line))
        for name, pattern in PATTERNS.items()
    }


def render_report(log_path: Path, counts: dict[str, int]) -> str:
    checks = [
        ("AnchorAttach", counts["attachments"] > 0),
        ("AttachedSurfaceTransition", counts["face_transitions"] > 0),
        ("BossBodyRebound", counts["body_rebounds"] > 0),
        ("ProtectedWeakPointNoDamage", counts["weakpoint_protected"] > 0),
        ("ExposedWeakPointDamage", counts["weakpoint_effective"] > 0),
        ("GroundShockwaveDamage", counts["player_damage"] > 0),
        ("PlayerDeath", counts["player_defeated"] > 0),
        ("BossDeath", counts["boss_dead_state"] > 0),
    ]
    rows = "\n".join(
        f"| {name} | {'PASS' if passed else 'NOT OBSERVED'} |"
        for name, passed in checks
    )
    metrics = "\n".join(
        f"- `{name}`: {value}" for name, value in counts.items()
    )
    return f"""# RDCA Combat Test Report

Source log: `{log_path}`

## Automated log checks

| Test | Result |
|---|---|
{rows}

## Event counts

{metrics}

## Manual checks

- [ ] Low arc movement feels responsive.
- [ ] High arc visibly clears the shockwave.
- [ ] Shockwave visual matches its damage timing.
- [ ] Charge, cooldown, HP, attachment, weak-point and Boss-state HUD values are readable.
"""


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=Path, help="Unreal log file to parse")
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        default=Path("docs/test_report.md"),
    )
    args = parser.parse_args()

    text = args.log.read_text(encoding="utf-8", errors="replace")
    report = render_report(args.log, parse_log(text))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(report, encoding="utf-8")
    print(f"Wrote {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
