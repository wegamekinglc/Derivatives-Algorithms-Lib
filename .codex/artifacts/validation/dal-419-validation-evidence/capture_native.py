import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import xml.etree.ElementTree as ET

evidence = Path(__file__).resolve().parent
root = Path('/mnt/d/multica-builds/dal419-tester-eb7d672c-696edf74')
win = 'D:/multica-builds/dal419-tester-eb7d672c-696edf74'
ninja = '/mnt/d/dev/ninja/ninja.exe'


def digest(path):
    with path.open('rb') as source:
        return hashlib.file_digest(source, 'sha256').hexdigest()


def copy(path):
    target = evidence / 'native' / path.relative_to(root)
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(path, target)


results = {}
for name in ['build-fixed', 'consumer-default', 'excel-default', 'focused-fixed', 'consumer-327', 'excel-327']:
    exit_code = int((root / (name + '.exit.txt')).read_text().strip())
    log = (root / (name + '.log')).read_text(errors='replace')
    assert exit_code == 0, (name, exit_code)
    assert '"system_acp": 936' in log and '"CL": null' in log and '"_CL_": null' in log, name
    version = '3.27.6' if name.endswith('-327') else '4.2.3'
    assert 'cmake version ' + version in log, name
    results[name] = {'exit': exit_code, 'native_cmake': version, 'system_acp': 936, 'CL': None, '_CL_': None}
    if name.startswith('consumer'):
        assert '100% tests passed, 0 tests failed out of 3' in log
        results[name]['passed'] = 3
    elif name.startswith('excel'):
        assert 'excel-OFF-build: passed' in log and 'excel-OFF-ctest: passed' in log
        assert 'excel-ON-build: passed' in log and 'excel-ON-ctest: passed' in log
        tests = (root / name / 'excel-ON-ctest.log').read_text(errors='replace')
        count = int(re.findall(r'100% tests passed, 0 tests failed out of (\d+)', tests)[-1])
        results[name]['passed'] = count
        results[name]['tests_OFF_XLL_and_audit'] = True
        results[name]['tests_ON_XLL_and_audit'] = True
        results[name]['ctest_elapsed'] = re.findall(r'Total Test time \(real\) =\s*([^\r\n]+)', tests)[-1]

probe = (root / 'master-probe.log').read_text(errors='replace')
assert int((root / 'master-probe.exit.txt').read_text()) != 0
assert 'error C2001' in probe and '"system_acp": 936' in probe
assert '/utf-8' not in probe
results['independent_master_header_RED'] = {'exit': int((root / 'master-probe.exit.txt').read_text()),
                                            'expected': 'C2001', 'native_cmake': '4.2.3', 'system_acp': 936}
focused = ET.parse(root / 'focused-fixed.xml').getroot()
assert focused.get('tests') == '42' and focused.get('failures') == '0'
results['focused-fixed']['passed'] = 42
actual = (root / 'compat-fixed.bin').read_bytes()
baseline = (evidence / 'author-windows-baseline.bin').read_bytes()
assert actual == baseline
results['rendering'] = {'all_66_symbols_and_widths_1_20_125_equal': True, 'empty_product_equal': True,
                        'bytes': len(actual), 'sha256': hashlib.sha256(actual).hexdigest()}

audited_commands = {}
for directory, target, version in [('consumer-default', 'dal_installed_consumer', '4.2.3'),
                                   ('consumer-327', 'dal_installed_consumer', '3.27.6'),
                                   ('excel-default/tests-ON', 'dal_excel_tests', '4.2.3'),
                                   ('excel-327/tests-ON', 'dal_excel_tests', '3.27.6'),
                                   ('focused-fixed', 'compat', '4.2.3')]:
    commands = subprocess.check_output([ninja, '-C', win + '/' + directory, '-t', 'commands', target])
    (evidence / (directory.replace('/', '-') + '-commands.log')).write_bytes(commands)
    lines = [line for line in commands.decode(errors='replace').splitlines()
             if re.search(r'\s[/-]c\s', line) and target + '.dir' in line]
    assert lines, directory
    assert not any(re.search(r'[/-]utf-8|(?:source|execution)-charset', line, re.I) for line in lines), directory
    cache = (root / directory / 'CMakeCache.txt').read_text()
    major, minor, patch = version.split('.')
    for field, value in [('MAJOR', major), ('MINOR', minor), ('PATCH', patch)]:
        assert 'CMAKE_CACHE_' + field + '_VERSION:INTERNAL=' + value in cache
    audited_commands[directory] = {'compile_commands_without_encoding_override': len(lines),
                                   'native_cmake': version}

raw_patterns = ('*.cmd', '*.log', '*.exit.txt', 'focused-fixed.xml', 'compat-fixed.bin', 'master-probe.cpp', 'probe_environment.py')
for pattern in raw_patterns:
    for path in root.glob(pattern):
        copy(path)
for directory in ['consumer-default', 'consumer-327', 'excel-default', 'excel-327', 'focused-fixed', 'source/build/native']:
    for path in (root / directory).rglob('*'):
        if path.is_file() and (path.suffix in ('.log', '.ninja') or path.name in ('CMakeCache.txt', 'compile_commands.json', 'installed-audit.log', 'runtime-contract.txt')):
            copy(path)

binaries = [root / 'install/lib/dal_cpp.lib', root / 'install/lib/dal_public.lib',
            root / 'focused-fixed/debugger_tests.exe', root / 'focused-fixed/compat.exe']
for directory in ['consumer-default', 'consumer-327']:
    binaries += list((root / directory).glob('*.exe'))
for directory in ['excel-default', 'excel-327']:
    binaries += [root / directory / 'tests-OFF/dal_excel.xll', root / directory / 'tests-ON/dal_excel.xll',
                 root / directory / 'tests-ON/dal_excel_tests.exe']
source = root / 'source'
header = 'dal-cpp/dal/script/visitor/debugger.hpp'
assert (source / header).read_bytes() == (root / 'install/include/dal/script/visitor/debugger.hpp').read_bytes()
manifest = {'tested_head': '696edf742f4c9024e7745f55c65923b56d98abca', 'results': results,
            'default_caller_commands': audited_commands,
            'source_audit': json.loads((evidence / 'source-audit.json').read_text()),
            'author_audit': json.loads((evidence / 'author-audit.json').read_text()),
            'snapshot': json.loads((evidence / 'native-snapshot.json').read_text()),
            'binary_sha256': {str(path.relative_to(root)): digest(path) for path in binaries},
            'source_header_sha256': digest(source / header),
            'installed_header_sha256': digest(root / 'install/include/dal/script/visitor/debugger.hpp')}
tool_paths = [root / 'tools/cmake-4.2.3-windows-x86_64/bin/cmake.exe',
              root / 'tools/cmake-4.2.3-windows-x86_64/bin/ctest.exe',
              Path('/mnt/c/Program Files/CMake/bin/cmake.exe'),
              Path('/mnt/c/Program Files/CMake/bin/ctest.exe'), Path(ninja),
              Path('/mnt/c/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/cl.exe')]
manifest['native_tool_sha256'] = {str(path): digest(path) for path in tool_paths}
(evidence / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
print(json.dumps({'results': results, 'commands': audited_commands, 'fresh_binaries': len(binaries)}, indent=2))
