import hashlib
import json
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile

evidence = Path(__file__).resolve().parent
report = evidence.parent / 'dal-418-rate-option-literals.md'
excluded = {'manifest.json', 'raw-evidence.zip', 'archive.json'}


def digest(path):
    with path.open('rb') as source:
        return hashlib.file_digest(source, 'sha256').hexdigest()


# Structured copies use repository line endings; raw Windows output stays in logs.
for path in evidence.glob('*.json'):
    data = path.read_bytes()
    if b'\r\n' in data:
        path.write_bytes(data.replace(b'\r\n', b'\n'))
manifest = json.loads((evidence / 'manifest.json').read_text())
manifest['evidence_sha256'] = {
    path.name: digest(path)
    for path in sorted(evidence.iterdir())
    if path.is_file() and path.name not in excluded
}
(evidence / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
members = [path for path in sorted(evidence.iterdir())
           if path.is_file() and path.name not in {'raw-evidence.zip', 'archive.json'}]
archive = evidence / 'raw-evidence.zip'
with ZipFile(archive, 'w', ZIP_DEFLATED, compresslevel=9) as bundle:
    for path in members:
        bundle.write(path, path.name)
with ZipFile(archive) as bundle:
    assert bundle.testzip() is None
    for name, expected in manifest['evidence_sha256'].items():
        assert hashlib.sha256(bundle.read(name)).hexdigest() == expected, name
summary = {
    'archive': archive.name,
    'sha256': digest(archive),
    'bytes': archive.stat().st_size,
    'members': len(members),
    'report': report.name,
    'report_sha256': digest(report),
    'tested_head': manifest['tested_head'],
    'verdict': 'AC1-AC7 PASS; AC8 blocked by two default Windows installed-package encoding failures',
}
(evidence / 'archive.json').write_text(json.dumps(summary, indent=2) + '\n')
print(json.dumps(summary, indent=2))
