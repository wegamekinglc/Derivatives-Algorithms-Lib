"""Exact native inputs admitted by the historical training/storage overlays."""

import gzip
import hashlib
import json
from pathlib import Path

from lsmc_storage_patch import CACHED_SOURCE_HASHES, SOURCE_HASHES


def native_sources(cached=False):
    commit = "9f626885e27e62a50599982c851274dc5289dd69" if cached else "05a24acd3457e0d42ce396f02d88572872d14095"
    hashes = CACHED_SOURCE_HASHES if cached else SOURCE_HASHES
    archive = Path(__file__).with_name("fixtures") / f"native-{commit[:8]}.json.gz"
    fixture = json.loads(gzip.decompress(archive.read_bytes()).decode("utf-8"))
    if fixture["source_commit"] != commit:
        raise ValueError("unexpected historical fixture commit")
    sources = fixture["sources"]
    if set(sources) != set(hashes):
        raise ValueError("incomplete historical fixture")
    for name, expected in hashes.items():
        if hashlib.sha256(sources[name].encode("utf-8")).hexdigest() != expected:
            raise ValueError(f"historical fixture identity mismatch: {name}")
    return sources
