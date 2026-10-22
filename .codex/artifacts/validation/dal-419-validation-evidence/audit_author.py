import hashlib
import json
from pathlib import Path
import re
import subprocess
from zipfile import ZipFile
import xml.etree.ElementTree as ET

evidence = Path(__file__).resolve().parent
repo = evidence.parents[3]
author = repo / '.codex/artifacts/implementation/dal-419-encoding-evidence'
manifest = json.loads((author / 'manifest.json').read_text())
archive_meta = json.loads((author / 'archive.json').read_text())


def sha(data):
    return hashlib.sha256(data).hexdigest()


def file_sha(path):
    with path.open('rb') as source:
        return hashlib.file_digest(source, 'sha256').hexdigest()


assert file_sha(author / 'raw-evidence.zip') == 'c78cc50718a93f6b35352876c7ac28b318a60736dfa0d77c1c47e1756580ffa1'
with ZipFile(author / 'raw-evidence.zip') as bundle:
    assert bundle.testzip() is None
    for name, expected in manifest['raw_sha256'].items():
        assert sha(bundle.read(name)) == expected, name
    for name, expected in manifest['metadata_sha256'].items():
        assert sha(bundle.read('local/' + name)) == expected, name
    for name in ['windows/master-probe-red.log', 'windows/consumer-red.log', 'windows/excel-red.log', 'windows/consumer-test-red.log']:
        assert b'error C2001' in bundle.read(name), name
    for name in ['windows/focused-baseline.xml', 'windows/focused-green.xml', 'local/linux-focused.xml']:
        xml = ET.fromstring(bundle.read(name))
        assert xml.get('tests') == '42' and xml.get('failures') == '0', name
    for mode in ['baseline', 'green']:
        audit = json.loads(bundle.read('local/source-audit-' + mode + '.json'))
        assert audit['checked'] == 5532 and not audit['mismatches']
    for name in ['consumer-red', 'excel-red', 'consumer-test-red', 'green', 'excel-green']:
        driver = bundle.read('windows/' + name + '.cmd').decode()
        assert 'if defined CL' in driver and 'if defined _CL_' in driver
        assert not re.search(r'/utf-8|(?:source|execution)-charset|chcp\s+\d+|set\s+"?(?:CL|_CL_)=', driver, re.I), name
    assert bundle.read('windows/compat-baseline.bin') == bundle.read('windows/compat-green.bin')
    windows_baseline = bundle.read('windows/compat-baseline.bin')
    assert windows_baseline.replace(b'\r\n', b'\n') == bundle.read('local/compat-linux.bin')
    (evidence / 'author-windows-baseline.bin').write_bytes(windows_baseline)
for filename, expected in manifest['binary_sha256'].items():
    assert file_sha(Path(filename)) == expected, filename

relative = 'dal-cpp/dal/script/visitor/debugger.hpp'
baseline = subprocess.check_output(['git', 'show', 'bf4cabea:' + relative], cwd=repo).decode()
fixed = (repo / relative).read_text()
# Lex comments, quoted strings and character constants before token comparison.
lex = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[A-Za-z_][A-Za-z_0-9]*|[^\s]', re.S)


def literal_bytes(token):
    text = token[1:-1]
    result = bytearray()
    escapes = {'a': 7, 'b': 8, 'f': 12, 'n': 10, 'r': 13, 't': 9, 'v': 11, '\\': 92, '"': 34, "'": 39, '?': 63}
    i = 0
    while i < len(text):
        if text[i] != '\\':
            result.extend(text[i].encode('utf-8'))
            i += 1
            continue
        i += 1
        if text[i] == 'x':
            match = re.match(r'x([0-9a-fA-F]+)', text[i:])
            result.append(int(match[1], 16))
            i += len(match[0])
        elif text[i] in '01234567':
            match = re.match(r'[0-7]{1,3}', text[i:])
            result.append(int(match[0], 8))
            i += len(match[0])
        else:
            result.append(escapes[text[i]])
            i += 1
    return bytes(result)


def tokens(source):
    return [token for token in lex.findall(source) if not token.startswith(('//', '/*'))]


old_tokens, new_tokens = tokens(baseline), tokens(fixed)
assert len(old_tokens) == len(new_tokens)
literal_count = changed = 0
for old, new in zip(old_tokens, new_tokens):
    if old.startswith('"'):
        literal_count += 1
        assert new.startswith('"') and literal_bytes(old) == literal_bytes(new), (old, new)
        changed += old != new
    else:
        assert old == new, (old, new)
assert changed == 23
parent_repo = repo.parents[2] / 'dal-408-e2075e8549aa/workdir/Derivatives-Algorithms-Lib'
parent_manifest = json.loads((parent_repo / '.codex/artifacts/validation/dal-418-validation-evidence/manifest.json').read_text())
old_cache = Path('/mnt/d/multica-builds/dal418-eb7d672c679c-6372ede9/source/build/Release-windows/CMakeCache.txt')
expected_cache = parent_manifest['windows_sha256']['source/build/Release-windows/CMakeCache.txt']
assert file_sha(old_cache) == expected_cache
cache = old_cache.read_text()
assert 'CMAKE_CACHE_MAJOR_VERSION:INTERNAL=3' in cache and 'CMAKE_CACHE_MINOR_VERSION:INTERNAL=27' in cache
assert 'CMAKE_CACHE_PATCH_VERSION:INTERNAL=6' in cache
result = {'author_raw_hashes_verified': len(manifest['raw_sha256']),
          'author_metadata_hashes_verified': len(manifest['metadata_sha256']),
          'author_binary_hashes_verified': len(manifest['binary_sha256']),
          'red_records_verified': 4, 'existing_42_case_xml_verified': 3,
          'all_cpp_tokens_and_literal_bytes_equal': True, 'actual_cpp_string_literals': literal_count,
          'changed_literal_representations': changed,
          'author_windows_baseline_bytes': len(windows_baseline), 'author_windows_baseline_sha256': sha(windows_baseline),
          'parent_cache_original_hash_matches': expected_cache,
          'parent_actual_native_cmake_version': '3.27.6',
          'correction': 'DAL-418 report incorrectly called native Windows CMake 4.2.3; its hashed cache proves 3.27.6. Linux used 4.2.3.'}
(evidence / 'author-audit.json').write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result, indent=2))
