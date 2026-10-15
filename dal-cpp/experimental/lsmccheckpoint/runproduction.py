"""Collect foreground, interleaved measurements of the production training providers."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
# Only fixed metadata queries and the operator-selected local driver are spawned below.
import subprocess  # nosec B404


ROOT = Path(__file__).resolve().parents[3]
MODES = ("hard", "frozen", "retrained")
SCHEDULES = ("materialized", "checkpoint")


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def executable(path):
    resolved = path.resolve(strict=True)
    require(resolved.is_file() and os.access(resolved, os.X_OK), "expected an executable local file")
    return resolved


def parse_options():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        args.binary = executable(args.binary)
    except (OSError, RuntimeError) as error:
        parser.error(f"cannot use local production driver: {error}")
    args.output = args.output.resolve()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    return args


def git_metadata(key):
    arguments = {"commit": ("rev-parse", "HEAD"), "status": ("status", "--porcelain")}
    flags = arguments[key]
    git = executable(Path("/usr/bin/git"))
    # A resolved system executable and an allowlisted argv; no caller-supplied git arguments.
    return subprocess.check_output(  # nosec B603
        [str(git), *flags], cwd=ROOT, text=True, shell=False)


def compiler_version():
    compiler = executable(Path("/usr/bin/c++"))
    # Query only the resolved system compiler's version, without PATH lookup or a shell.
    return subprocess.check_output(  # nosec B603
        [str(compiler), "--version"], text=True, shell=False).splitlines()[0]


def collect_sample(binary, raw_dir, mode, sample, schedule):
    require(mode in MODES and schedule in SCHEDULES, "unsupported production workload")
    require(isinstance(sample, int) and 0 <= sample < 10, "invalid sample index")
    binary = executable(binary)
    timer = executable(Path("/usr/bin/time"))
    label = f"{mode}-{sample}-{schedule}"
    resource = (raw_dir / f"{label}.time").resolve()
    command = [str(binary), "--schedule", schedule, "--mode", mode,
               "--events", "65", "--block", "8", "--training", "4097", "--validation", "1023",
               "--pricing", "2049", "--bridge", "true"]
    # The operator trusts this local driver; both executables and the log path are absolute.
    # Workload arguments are fixed or allowlisted above, and shell metacharacters stay in argv.
    process = subprocess.run(  # nosec B603
        [str(timer), "-v", "-o", str(resource), *command],
        cwd=ROOT, env=dict(os.environ, DAL_NUM_THREADS="1"),
        text=True, capture_output=True, check=False, shell=False,
    )
    (raw_dir / f"{label}.out").write_text(process.stdout)
    (raw_dir / f"{label}.err").write_text(process.stderr)
    require(process.returncode == 0, f"{label} exited {process.returncode}")
    result = json.loads(process.stdout.splitlines()[-1])
    usage = resource.read_text()
    rss = next(int(line.split(":")[-1]) for line in usage.splitlines()
               if "Maximum resident set size" in line)
    result.update(sample=sample, peak_rss_kib=rss, stdout=process.stdout, stderr=process.stderr, resource_output=usage)
    return result


def validate_pair(pair, mode):
    require(pair["materialized"]["pv"] == pair["checkpoint"]["pv"], "price mismatch")
    require(pair["materialized"]["risks"] == pair["checkpoint"]["risks"], "named risk mismatch")
    fits = 11 if mode == "retrained" else 1
    require(pair["checkpoint"]["normal_vectors"] == 5120 * 10 * fits, "normal-vector work bound mismatch")
    require(pair["checkpoint"]["model_steps"] == 5120 * 65 * 2 * fits, "model-step work bound mismatch")
    require(pair["materialized"]["normal_vectors"] == 5121 * fits, "native work bound mismatch")
    require(pair["checkpoint"]["allocation_peak_bytes"] <= pair["checkpoint"]["allocation_bound_bytes"], "allocation bound exceeded")


def collect_pairs(binary):
    samples = []
    raw_dir = ROOT / "build/checkpoint-production-raw"
    raw_dir.mkdir(parents=True, exist_ok=True)
    for mode in MODES:
        for sample in range(10):
            pair = {}
            order = SCHEDULES if sample % 2 == 0 else SCHEDULES[::-1]
            for schedule in order:
                result = collect_sample(binary, raw_dir, mode, sample, schedule)
                samples.append(result)
                pair[schedule] = result
            validate_pair(pair, mode)
            print(f"{mode} pair {sample + 1}/10: exact price/risks; "
                  f"full={pair['materialized']['seconds']:.4f}s checkpoint={pair['checkpoint']['seconds']:.4f}s",
                  flush=True)
    return samples


def summarize(samples, mode, schedule):
    selected = [s for s in samples if (s["mode"], s["schedule"]) == (mode, schedule)]
    return {
        "samples": len(selected),
        "minimum_seconds": min(s["seconds"] for s in selected),
        "minimum_peak_rss_kib": min(s["peak_rss_kib"] for s in selected),
        "maximum_charged_peak_bytes": max(s["allocation_peak_bytes"] for s in selected),
        "maximum_retained_tape_bytes": max(s["retained_tape_bytes"] for s in selected),
        "maximum_post_pricing_tape_bytes": max(s["post_pricing_tape_bytes"] for s in selected),
    }


def payload(identity, samples):
    return {
        "source_commit": git_metadata("commit").strip(),
        "source_status": git_metadata("status"),
        "executable_sha256": identity,
        "compiler": compiler_version(),
        "platform": platform.platform(),
        "threads": 1,
        "configuration": {"events": 65, "block": 8, "training": 4097, "validation": 1023,
                          "pricing": 2049, "pricing_replicates": 2, "bridge": True,
                          "training_seed": 17, "pricing_seed": 29, "degree": 2},
        "risk_names": ["spot", "vol", "rate", "div", "K"],
        "timing_scope": "Informational provider comparison on a shared host; no regression or financial-tolerance verdict.",
        "summary": {mode: {schedule: summarize(samples, mode, schedule) for schedule in SCHEDULES} for mode in MODES},
        "samples": samples,
    }


def main():
    args = parse_options()
    identity = digest(args.binary)
    samples = collect_pairs(args.binary)
    require(digest(args.binary) == identity, "executable identity changed during measurement")
    args.output.write_text(json.dumps(payload(identity, samples), indent=2) + "\n")


if __name__ == "__main__":
    main()
