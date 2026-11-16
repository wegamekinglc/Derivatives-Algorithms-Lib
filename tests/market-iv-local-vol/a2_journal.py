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
    return prior_entry


def entry(row, previous_entry_hash, transition):
    row["previous_entry_sha256"] = previous_entry_hash
    if transition:
        row["transition"] = transition
    row["entry_sha256"] = seal(row)
    return row
