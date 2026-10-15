"""Collect foreground, interleaved measurements of the production training providers."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    binary = args.binary
    identity = digest(binary)
    environment = dict(os.environ, DAL_NUM_THREADS="1")
    samples = []
    args.output.parent.mkdir(parents=True, exist_ok=True)
    raw_dir = Path("build/checkpoint-production-raw")
    raw_dir.mkdir(parents=True, exist_ok=True)
    common = ["--events", "65", "--block", "8", "--training", "4097", "--validation", "1023",
              "--pricing", "2049", "--bridge", "true"]
    for mode in ("hard", "frozen", "retrained"):
        for sample in range(10):
            pair = {}
            order = ("materialized", "checkpoint") if sample % 2 == 0 else ("checkpoint", "materialized")
            for schedule in order:
                label = f"{mode}-{sample}-{schedule}"
                resource = raw_dir / f"{label}.time"
                command = [str(binary), "--schedule", schedule, "--mode", mode, *common]
                process = subprocess.run(
                    ["/usr/bin/time", "-v", "-o", str(resource), *command],
                    env=environment, text=True, capture_output=True, check=False,
                )
                (raw_dir / f"{label}.out").write_text(process.stdout)
                (raw_dir / f"{label}.err").write_text(process.stderr)
                require(process.returncode == 0, f"{label} exited {process.returncode}")
                result = json.loads(process.stdout)
                usage = resource.read_text()
                rss = next(int(line.split(":")[-1]) for line in usage.splitlines()
                           if "Maximum resident set size" in line)
                result.update(sample=sample, peak_rss_kib=rss, stdout=process.stdout, stderr=process.stderr, resource_output=usage)
                samples.append(result)
                pair[schedule] = result
            require(pair["materialized"]["pv"] == pair["checkpoint"]["pv"], "price mismatch")
            require(pair["materialized"]["risks"] == pair["checkpoint"]["risks"], "named risk mismatch")
            fits = 11 if mode == "retrained" else 1
            require(pair["checkpoint"]["normal_vectors"] == 5120 * 10 * fits, "normal-vector work bound mismatch")
            require(pair["checkpoint"]["model_steps"] == 5120 * 65 * 2 * fits, "model-step work bound mismatch")
            require(pair["materialized"]["normal_vectors"] == 5121 * fits, "native work bound mismatch")
            require(pair["checkpoint"]["allocation_peak_bytes"] <= pair["checkpoint"]["allocation_bound_bytes"], "allocation bound exceeded")
            print(f"{mode} pair {sample + 1}/10: exact price/risks; "
                  f"full={pair['materialized']['seconds']:.4f}s checkpoint={pair['checkpoint']['seconds']:.4f}s",
                  flush=True)
    require(digest(binary) == identity, "executable identity changed during measurement")
    summary = {}
    for mode in ("hard", "frozen", "retrained"):
        summary[mode] = {}
        for schedule in ("materialized", "checkpoint"):
            selected = [s for s in samples if s["mode"] == mode and s["schedule"] == schedule]
            summary[mode][schedule] = {
                "samples": len(selected),
                "minimum_seconds": min(s["seconds"] for s in selected),
                "minimum_peak_rss_kib": min(s["peak_rss_kib"] for s in selected),
                "maximum_charged_peak_bytes": max(s["allocation_peak_bytes"] for s in selected),
                "maximum_retained_tape_bytes": max(s["retained_tape_bytes"] for s in selected),
                "maximum_post_pricing_tape_bytes": max(s["post_pricing_tape_bytes"] for s in selected),
            }
    payload = {
        "source_commit": subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip(),
        "source_status": subprocess.check_output(["git", "status", "--porcelain"], text=True),
        "executable_sha256": identity,
        "compiler": subprocess.check_output(["c++", "--version"], text=True).splitlines()[0],
        "platform": platform.platform(),
        "threads": 1,
        "configuration": {"events": 65, "block": 8, "training": 4097, "validation": 1023,
                          "pricing": 2049, "pricing_replicates": 2, "bridge": True,
                          "training_seed": 17, "pricing_seed": 29, "degree": 2},
        "risk_names": ["spot", "vol", "rate", "div", "K"],
        "timing_scope": "Informational provider comparison on a shared host; no regression or financial-tolerance verdict.",
        "summary": summary,
        "samples": samples,
    }
    args.output.write_text(json.dumps(payload, indent=2) + "\n")


if __name__ == "__main__":
    main()
