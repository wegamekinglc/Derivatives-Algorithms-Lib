from pathlib import Path
import shutil

evidence = Path(__file__).resolve().parent
root = Path('/mnt/d/multica-builds/dal419-tester-eb7d672c-696edf74')
shutil.copyfile(evidence / 'probe_environment.py', root / 'probe_environment.py')
for path in root.glob('*.cmd'):
    data = path.read_bytes().replace(b'ctest --version\r\n', b'ctest --version\r\n"D:/multica-builds/dal418-eb7d672c679c-6372ede9/source/dal-python/.venv/Scripts/python.exe" "D:/multica-builds/dal419-tester-eb7d672c-696edf74/probe_environment.py"\r\nif errorlevel 1 exit /b 95\r\n')
    path.write_bytes(data)
    (evidence / path.name).write_bytes(data)
build_driver = root / 'build-fixed.cmd'
data = build_driver.read_bytes().replace(b'-DDAL_CPP_BUILD_TESTS=OFF',
                                       b'-DDAL_CPP_BUILD_TESTS=OFF -DDAL_CPP_BUILD_BENCHMARKS=OFF')
build_driver.write_bytes(data)
(evidence / 'build-fixed.cmd').write_bytes(data)
for kind in ('consumer', 'excel'):
    old = kind + '-default'
    new = kind + '-327'
    data = (root / (old + '.cmd')).read_bytes()
    data = data.replace(old.encode(), new.encode())
    data = data.replace(b'D:/multica-builds/dal419-tester-eb7d672c-696edf74/tools/cmake-4.2.3-windows-x86_64/bin',
                        b'C:/Program Files/CMake/bin')
    (root / (new + '.cmd')).write_bytes(data)
    (evidence / (new + '.cmd')).write_bytes(data)
