"""Exact native inputs admitted by the historical training/storage overlays."""

import gzip
import hashlib
import json
from pathlib import Path

from lsmc_storage_patch import SOURCE_HASHES


def native_sources():
    archive = Path(__file__).with_name("fixtures") / "native-05a24acd.json.gz"
    fixture = json.loads(gzip.decompress(archive.read_bytes()).decode("utf-8"))
    if fixture["source_commit"] != "05a24acd3457e0d42ce396f02d88572872d14095":
        raise ValueError("unexpected historical fixture commit")
    sources = fixture["sources"]
    if set(sources) != set(SOURCE_HASHES):
        raise ValueError("incomplete historical fixture")
    for name, expected in SOURCE_HASHES.items():
        if hashlib.sha256(sources[name].encode("utf-8")).hexdigest() != expected:
            raise ValueError(f"historical fixture identity mismatch: {name}")
    return sources
