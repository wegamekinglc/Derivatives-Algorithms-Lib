import hashlib
import json
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile

evidence = Path(__file__).resolve().parent
report = evidence.parent / 'dal-419-installed-windows-encoding.md'
excluded = {'manifest.json', 'raw-evidence.zip', 'archive.json'}


def digest(path):
    with path.open('rb') as source:
        return hashlib.file_digest(source, 'sha256').hexdigest()


files = [path for path in sorted(evidence.rglob('*'))
         if path.is_file() and path.name not in excluded and '__pycache__' not in path.parts]
manifest = json.loads((evidence / 'manifest.json').read_text())
if (evidence / 'master-advance.json').exists():
    manifest['latest_master_observation_not_integrated'] = json.loads((evidence / 'master-advance.json').read_text())
manifest['raw_evidence_sha256'] = {str(path.relative_to(evidence)): digest(path) for path in files}
(evidence / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
files.append(evidence / 'manifest.json')
archive = evidence / 'raw-evidence.zip'
with ZipFile(archive, 'w', ZIP_DEFLATED, compresslevel=9) as bundle:
    for path in files:
        bundle.write(path, str(path.relative_to(evidence)))
with ZipFile(archive) as bundle:
    assert bundle.testzip() is None
    for name, expected in manifest['raw_evidence_sha256'].items():
        assert hashlib.sha256(bundle.read(name)).hexdigest() == expected, name
summary = {'tested_head': manifest['tested_head'], 'verdict': 'PASS for scoped DAL-419 validation; review/PR/final-head gates pending',
           'archive': archive.name, 'archive_sha256': digest(archive), 'archive_bytes': archive.stat().st_size,
           'raw_hash_count': len(files) - 1, 'report_sha256': digest(report)}
(evidence / 'archive.json').write_text(json.dumps(summary, indent=2) + '\n')
print(json.dumps(summary, indent=2))
