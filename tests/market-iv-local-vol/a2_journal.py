"""Validate both the legacy state chain and new full-entry journal hashes."""

import hashlib
import json


def seal(value):
    return hashlib.sha256(
        json.dumps(value, sort_keys=True, separators=(",", ":")).encode()
    ).hexdigest()


def checked_entry(row, prior):
    if "entry_sha256" in row:
        content = {k: v for k, v in row.items() if k != "entry_sha256"}
        if seal(content) != row["entry_sha256"]:
            raise RuntimeError("Cumulative journal entry hash mismatch")
        if row["previous_entry_sha256"] != prior:
            raise RuntimeError("Cumulative journal entry chain mismatch")


def validate(path, state_hash, state):
    previous, prior_entry = None, None
    lines = path.read_text().splitlines()
    for index, line in enumerate(lines, 1):
        row = json.loads(line)
        if row["sequence"] != index or row["previous_hash"] != previous:
            raise RuntimeError("Cumulative journal state chain mismatch")
        checked_entry(row, prior_entry)
        previous = row["state_sha256"]
        prior_entry = hashlib.sha256(line.encode()).hexdigest()
    if previous != state_hash or len(lines) != state["sequence"]:
        raise RuntimeError("Cumulative budget journal mismatch")
    transition = state.get("transition")
    if transition:
        prefix = "\n".join(lines[: transition["origin_sequence"]]) + "\n"
        if hashlib.sha256(prefix.encode()).hexdigest() != transition["journal"]:
            raise RuntimeError("Original journal prefix changed")


def entry(row, journal, transition):
    lines = journal.read_text().splitlines() if journal.exists() else []
    previous = lines[-1] if lines else None
    row["previous_entry_sha256"] = (
        hashlib.sha256(previous.encode()).hexdigest() if previous else None
    )
    if transition:
        row["transition"] = transition
    row["entry_sha256"] = seal(row)
    return row
