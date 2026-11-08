"""Prefill the pinned cibuildwheel virtualenv cache without retrying wheel builds."""

import argparse
import hashlib
import http.client
import os
from pathlib import Path
import shutil
import ssl
import sys
import tempfile
import time
import urllib.error
import urllib.request


CIBUILDWHEEL_REVISION = "f9b966cfadf4109556cadc5bb39ddb72d0ce664e"
VIRTUALENV_VERSION = "21.14.13"
VIRTUALENV_SHA256 = "215e29d6c8e2a4bf5f509828368629bbe7eddb847dcfee862db0f84e6df40047"
URLS = (
    "https://github.com/pypa/get-virtualenv/releases/download/21.14.13/virtualenv.pyz",
    "https://api.github.com/repos/pypa/get-virtualenv/releases/assets/666956820",
)
ATTEMPTS = 6
TIMEOUT = 30


def verify(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(65536), b""):
            digest.update(chunk)
    observed = digest.hexdigest()
    if observed != VIRTUALENV_SHA256:
        raise ValueError("SHA256 mismatch for %s: expected %s, got %s" % (path, VIRTUALENV_SHA256, observed))


def fetch(target, url):
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=target.parent, suffix=".part", delete=False) as output:
            temporary = Path(output.name)
            request = urllib.request.Request(url, headers={"Accept": "application/octet-stream", "User-Agent": "DAL-wheel-preparation"})
            with urllib.request.urlopen(request, timeout=TIMEOUT) as response:
                shutil.copyfileobj(response, output)
        verify(temporary)
        temporary.replace(target)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def transient(error):
    if isinstance(error, urllib.error.HTTPError):
        return error.code in (408, 429, 500, 502, 503, 504)
    return not isinstance(getattr(error, "reason", None), ssl.SSLCertVerificationError)


def prepare(cache):
    cache = Path(cache).resolve()
    cache.mkdir(parents=True, exist_ok=True)
    target = cache / ("virtualenv-%s.pyz" % VIRTUALENV_VERSION)
    if target.exists():
        verify(target)
        return target
    for attempt in range(ATTEMPTS):
        try:
            fetch(target, URLS[attempt % len(URLS)])
            return target
        except (urllib.error.URLError, TimeoutError, ConnectionError, http.client.IncompleteRead) as error:
            if not transient(error) or attempt == ATTEMPTS - 1:
                raise
            delay = 3 * 2 ** attempt
            print("virtualenv download attempt %d/%d failed: %s; retry in %ds" % (attempt + 1, ATTEMPTS, error, delay), file=sys.stderr)
            time.sleep(delay)
    raise RuntimeError("virtualenv acquisition did not complete")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cache-dir", required=True, type=Path)
    args = parser.parse_args(argv)
    cache = args.cache_dir.resolve()
    if any(char in str(cache) for char in "\r\n"):
        raise ValueError("cache path must occupy one environment-file line")
    target = prepare(cache)
    environment_file = os.environ.get("GITHUB_ENV")
    if environment_file:
        with open(environment_file, "a", encoding="utf-8") as output:
            output.write("CIBW_CACHE_PATH=%s\n" % cache)
    print("Verified %s (SHA256 %s)" % (target, VIRTUALENV_SHA256))


if __name__ == "__main__":
    main()
