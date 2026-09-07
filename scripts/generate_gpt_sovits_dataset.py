#!/usr/bin/env python3
"""Build a conservative GPT-SoVITS training list from the Hyori voice index."""

from __future__ import annotations

import argparse
import json
import re
from dataclasses import dataclass
from pathlib import Path

import soundfile as sf


DEFAULT_EXCLUDE_PATTERN = re.compile(
    "|".join(
        re.escape(term)
        for term in (
            # Strong vocalisations and non-neutral delivery.
            "あああ",
            "ううう",
            "おおお",
            "んんん",
            "ふぁ",
            "ひゃ",
            "きゃ",
            "喘",
            "叫",
            "泣",
            "絶叫",
            # Material unsuitable for the default companion voice model.
            "えっち",
            "エッチ",
            "キス",
            "触ら",
            "舐め",
            "挿",
            "裸",
            "胸",
            "おっぱい",
            "下着",
            "ベッド",
            "気持ちいい",
            "感じちゃ",
            "イク",
            "いくぅ",
        )
    )
)
REPEATED_CHARACTER_PATTERN = re.compile(r"(.)\1{3,}")
JAPANESE_CHARACTER_PATTERN = re.compile(r"[\u3040-\u30ff\u3400-\u9fff]")


@dataclass(frozen=True)
class Sample:
    path: Path
    text: str
    duration: float
    score: float


def normalize_text(value: str) -> str:
    return re.sub(r"\s+", " ", value).strip()


def quality_score(text: str, duration: float) -> float:
    score = 100.0
    score -= abs(duration - 5.0) * 4.0
    score -= abs(len(text) - 28) * 0.35

    if text.endswith(("。", "？", "！", "?", "!")):
        score += 4.0
    if "……" in text:
        score -= min(text.count("……") * 1.5, 6.0)
    if text.count("、") + text.count("，") > 4:
        score -= 3.0
    return score


def rejection_reason(text: str, duration: float) -> str | None:
    if not 2.0 <= duration <= 10.0:
        return "duration"
    if not 8 <= len(text) <= 72:
        return "text_length"
    if "|" in text:
        return "delimiter"
    if len(JAPANESE_CHARACTER_PATTERN.findall(text)) < 6:
        return "too_little_japanese"
    if DEFAULT_EXCLUDE_PATTERN.search(text):
        return "excluded_content"
    if REPEATED_CHARACTER_PATTERN.search(text):
        return "repeated_vocalisation"
    return None


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--index", type=Path, required=True)
    parser.add_argument("--voice-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--speaker", default="hyori")
    parser.add_argument("--language", default="ja")
    parser.add_argument("--max-samples", type=int, default=250)
    parser.add_argument("--max-minutes", type=float, default=25.0)
    args = parser.parse_args()

    index_path = args.index.resolve()
    voice_root = args.voice_root.resolve()
    entries = json.loads(index_path.read_text(encoding="utf-8"))

    accepted: list[Sample] = []
    rejected: dict[str, int] = {}
    missing = 0

    for entry in entries:
        relative_path = str(entry.get("path", "")).replace("/", str(Path("/")))
        text = normalize_text(str(entry.get("text", "")))
        audio_path = (voice_root / relative_path).resolve()
        if not audio_path.is_file():
            missing += 1
            continue

        try:
            duration = float(sf.info(str(audio_path)).duration)
        except Exception:
            rejected["unreadable_audio"] = rejected.get("unreadable_audio", 0) + 1
            continue

        reason = rejection_reason(text, duration)
        if reason:
            rejected[reason] = rejected.get(reason, 0) + 1
            continue

        accepted.append(
            Sample(audio_path, text, duration, quality_score(text, duration))
        )

    # Rank conservatively, then preserve the original voice-number order in the list.
    accepted.sort(key=lambda sample: (-sample.score, sample.path.name))
    selected: list[Sample] = []
    selected_seconds = 0.0
    max_seconds = args.max_minutes * 60.0
    for sample in accepted:
        if len(selected) >= args.max_samples:
            break
        if selected and selected_seconds + sample.duration > max_seconds:
            continue
        selected.append(sample)
        selected_seconds += sample.duration

    selected.sort(key=lambda sample: sample.path.name)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.report.parent.mkdir(parents=True, exist_ok=True)

    lines = [
        f"{sample.path.as_posix()}|{args.speaker}|{args.language}|{sample.text}"
        for sample in selected
    ]
    args.output.write_text("\n".join(lines) + "\n", encoding="utf-8")

    durations = [sample.duration for sample in selected]
    report = {
        "source_index": str(index_path),
        "voice_root": str(voice_root),
        "output_list": str(args.output.resolve()),
        "source_entries": len(entries),
        "eligible_entries": len(accepted),
        "selected_entries": len(selected),
        "selected_minutes": round(selected_seconds / 60.0, 2),
        "minimum_duration_seconds": round(min(durations), 3) if durations else 0,
        "maximum_duration_seconds": round(max(durations), 3) if durations else 0,
        "average_duration_seconds": (
            round(selected_seconds / len(selected), 3) if selected else 0
        ),
        "missing_audio": missing,
        "rejected": dict(sorted(rejected.items())),
    }
    args.report.write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return 0 if selected else 1


if __name__ == "__main__":
    raise SystemExit(main())
