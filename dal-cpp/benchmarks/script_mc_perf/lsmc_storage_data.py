"""Storage-specific matrices and strict native metadata admission."""

from lsmc_study_data import parse_record, profile_definition, profile_jobs


def storage_definition(profile):
    if profile not in ("accuracy", "corner"):
        raise ValueError("storage profile must be accuracy or corner")
    budgets = (512, 32768) if profile == "accuracy" else (65536, 524288)
    return dict(profile_definition(profile), budgets=budgets)


def storage_jobs(profile, threads=4):
    definition = storage_definition(profile)
    return tuple(job for job in profile_jobs(profile, threads=threads) if job.training in definition["budgets"])


def require_width(record, bits):
    if type(bits) is not int or bits not in (32, 64) or type(record.get("storage_bits")) is not int or record["storage_bits"] != bits:
        raise ValueError("native result storage width differs from the experiment image")


def parse_storage_record(output, job, anchor, bits):
    record = parse_record(output, job, anchor)
    require_width(record, bits)
    if type(record.get("peak_training_row_bytes")) is not int or record["peak_training_row_bytes"] <= 0:
        raise ValueError("native result lacks a positive measured row allocation")
    if type(record.get("live_training_row_bytes")) is not int or record["live_training_row_bytes"] != 0:
        raise ValueError("native result retains training rows after valuation")
    return record
