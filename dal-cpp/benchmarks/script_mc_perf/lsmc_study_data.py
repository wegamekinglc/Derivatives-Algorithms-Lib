"""Prespecified study jobs and strict native-result admission."""

from dataclasses import asdict, dataclass
import hashlib
import itertools
import json
import math

from lsmc_study_support import block_layout

TRAINING_SEEDS = (17, 41, 73, 101, 149, 211, 307, 401)
PRICING_SEEDS = (1009, 2027, 3001, 4051, 5003, 6089, 7013, 8087)
CASES = ("european_put", "bermudan_atm", "bermudan_itm", "bermudan_otm", "bermudan_low_vol",
         "weekly_put", "daily_put", "weekly_call", "asian_two", "asian_three", "local_vol_weekly")


@dataclass(frozen=True)
class Job:
    case: str
    training: int
    training_seed: int
    pricing_seed: int
    mode: str
    pricing: int = 4096
    validation: int = 128
    engine: str = "compiled"
    precision: str = "Default"
    bridge: bool = False
    threads: int = 4
    replicates: int = 2

    @property
    def key(self):
        text = json.dumps(asdict(self), sort_keys=True, separators=(",", ":"))
        return hashlib.sha256(text.encode()).hexdigest()

    def command(self, executable):
        options = {
            "case": self.case, "training": self.training, "pricing": self.pricing,
            "validation": self.validation, "training-seed": self.training_seed,
            "pricing-seed": self.pricing_seed, "mode": self.mode, "engine": self.engine,
            "normal-precision": self.precision, "brownian-bridge": str(self.bridge).lower(),
            "threads": self.threads, "replicates": self.replicates, "diagnostics": "true",
        }
        return [str(executable), "--lsmc-training-sample", *itertools.chain.from_iterable(
            ("--" + name, str(value)) for name, value in options.items())]


def profile_definition(name):
    profiles = {
        "accuracy": dict(cases=CASES, budgets=(512, 2048, 8192, 32768), training_seeds=TRAINING_SEEDS,
                         pricing_seeds=PRICING_SEEDS, pricing=4096, validation=128, anchor=32768, diagnostic_only=False),
        "corner": dict(cases=("daily_put",), budgets=(65536, 131072, 524288), training_seeds=TRAINING_SEEDS,
                       pricing_seeds=PRICING_SEEDS, pricing=32768, validation=1024, anchor=524288, diagnostic_only=False),
        "smoke": dict(cases=("european_put", "asian_three", "local_vol_weekly"), budgets=(64, 512),
                      training_seeds=TRAINING_SEEDS[:2], pricing_seeds=PRICING_SEEDS[:2], pricing=257,
                      validation=16, anchor=32768, diagnostic_only=True),
        "calibration": dict(cases=("daily_put", "asian_three", "local_vol_weekly"), budgets=(32768,),
                            training_seeds=TRAINING_SEEDS[:1], pricing_seeds=PRICING_SEEDS[:1], pricing=4096,
                            validation=128, anchor=32768, diagnostic_only=True),
    }
    if name not in profiles:
        raise ValueError(f"unknown study profile: {name}")
    return profiles[name]


def profile_jobs(name, cases=None, threads=4):
    profile = profile_definition(name)
    selected = tuple(cases) if cases is not None else profile["cases"]
    if not selected or len(set(selected)) != len(selected) or any(case not in profile["cases"] for case in selected):
        raise ValueError("case selection must be a nonempty unique subset of the profile")
    combinations = itertools.product(selected, profile["budgets"], profile["training_seeds"], profile["pricing_seeds"], ("frozen", "retrained"))
    return tuple(Job(case, budget, training, pricing, mode, pricing=profile["pricing"], validation=profile["validation"], threads=threads)
                 for case, budget, training, pricing, mode in combinations)


def expected_fields(job, anchor):
    layout = block_layout(anchor, job.training, job.validation, job.pricing, job.replicates)
    return {
        "record": "lsmc_training_sample", "addressing": "controlled", "case": job.case,
        "engine": job.engine, "mode": job.mode, "training_paths": job.training,
        "pricing_paths": job.pricing, "validation_paths": job.validation,
        "validation_offset": layout["validation"][0], "pricing_offset": layout["pricing"][0][0],
        "replicates": job.replicates, "training_seed": job.training_seed, "pricing_seed": job.pricing_seed,
        "threads": job.threads, "normal_precision": job.precision, "brownian_bridge": job.bridge, "diagnostics": True,
    }


def finite_numbers(values):
    return all(type(value) in (int, float) and math.isfinite(value) for value in values)


def validate_risks(record, job):
    names, risks = record.get("risk_names", []), record.get("risks", [])
    expected = 8 if job.case == "local_vol_weekly" else 5
    if len(names) != expected or len(risks) != expected or len(set(names)) != expected:
        raise ValueError("missing, duplicated or cropped native risk coordinates")
    if not valid_risk_names(names) or not finite_numbers([record.get("pv"), *risks]):
        raise ValueError("invalid native price/risk values")


def valid_risk_names(names):
    return all(isinstance(name, str) and name for name in names)


def validate_diagnostics(record, job):
    replicates = record.get("hard_replicate_means", [])
    if len(replicates) != job.replicates or not finite_numbers([record.get("hard_pv"), *replicates]):
        raise ValueError("missing hard-policy replicate diagnostics")
    if not isinstance(record.get("policy"), list) or not record["policy"]:
        raise ValueError("missing fitted-policy diagnostics")


def parse_record(output, job, anchor):
    records = [json.loads(line) for line in output.splitlines() if line.startswith("{")]
    if len(records) != 1:
        raise ValueError("expected exactly one native result record")
    record = records[0]
    if any(record.get(name) != value for name, value in expected_fields(job, anchor).items()):
        raise ValueError("native result settings or sample addresses do not match the job")
    validate_risks(record, job)
    validate_diagnostics(record, job)
    return record


def risk_vector(record):
    return {"PV": record["pv"], **{"d_" + name: value for name, value in zip(record["risk_names"], record["risks"])}}
