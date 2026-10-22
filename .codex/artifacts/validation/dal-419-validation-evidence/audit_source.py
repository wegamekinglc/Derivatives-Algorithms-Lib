import hashlib
import json
from pathlib import Path
import subprocess

evidence = Path(__file__).resolve().parent
repo = evidence.parents[3]
root = Path('/mnt/d/multica-builds/dal419-tester-eb7d672c-696edf74/source')
head = '696edf742f4c9024e7745f55c65923b56d98abca'
snapshot = json.loads((evidence / 'native-snapshot.json').read_text())
records, eol, skipped = [], [], []


def audit(repository, revision, prefix=''):
    listing = subprocess.check_output(['git', 'ls-tree', '-rz', revision], cwd=repository)
    for entry in listing.split(b'\0'):
        if not entry:
            continue
        metadata, raw_name = entry.split(b'\t', 1)
        mode, kind, expected = metadata.decode().split()
        relative = prefix + raw_name.decode()
        if kind != 'blob':
            skipped.append(relative)
            continue
        data = (root / relative).read_bytes()
        actual = hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()
        if actual != expected:
            normalized = data.replace(b'\r\n', b'\n')
            actual = hashlib.sha1(b'blob ' + str(len(normalized)).encode() + b'\0' + normalized).hexdigest()
            assert actual == expected, relative
            eol.append(relative)
        records.append([relative, expected, hashlib.sha256(data).hexdigest()])


audit(repo, head)
for name, dependency in snapshot['dependencies'].items():
    prefix = 'dal-cpp/externals/' + name
    actual_pin = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=repo / prefix, text=True).strip()
    assert actual_pin == dependency['pin']
    audit(repo / prefix, dependency['pin'], prefix + '/')
for relative in ['dal-cpp/dal/script/visitor/debugger.hpp', 'tests/installed-consumer/main.cpp']:
    assert (root / relative).read_bytes() == (repo / relative).read_bytes()
result = {'head': head, 'verified_git_blobs': len(records), 'content_mismatches': [],
          'eol_only': eol, 'gitlink_entries_skipped_in_tree_listing': skipped,
          'dependency_contents_separately_verified': list(snapshot['dependencies']),
          'aggregate_sorted_records_sha256': hashlib.sha256(json.dumps(sorted(records)).encode()).hexdigest()}
(evidence / 'source-audit.json').write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result, indent=2))
