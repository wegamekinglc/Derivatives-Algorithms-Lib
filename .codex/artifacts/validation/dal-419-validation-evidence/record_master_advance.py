from datetime import datetime, timezone
import json
from pathlib import Path
import subprocess

evidence = Path(__file__).resolve().parent
repo = evidence.parents[3]


def git(*args):
    return subprocess.check_output(['git', *args], cwd=repo, text=True).strip()


base = 'bf4cabea8575206375047e51993a5aa5aeb54c0c'
tested = '696edf742f4c9024e7745f55c65923b56d98abca'
assert git('rev-parse', 'HEAD') == tested
master = git('rev-parse', 'origin/master')
changes = git('diff', '--name-status', base, master)
(evidence / 'master-advance.log').write_text(changes + '\n')
unchanged = ['dal-cpp/dal/script/visitor/debugger.hpp', 'tests/installed-consumer',
             'tests/optional-excel/windows-installed.cmake', 'dal-excel/CMakeLists.txt',
             'CMakeLists.txt', '.gitmodules', 'dal-cpp/externals']
assert not git('diff', '--name-only', base, master, '--', *unchanged)
result = {'observed_utc': datetime.now(timezone.utc).isoformat(), 'tested_head': tested,
          'tested_base': base, 'latest_master_observed': master,
          'new_commit': git('log', '--format=%H %s', base + '..' + master),
          'changed_files': len(changes.splitlines()), 'encoding_fix_and_drivers_unchanged': unchanged,
          'new_product_scope': ['calibration protocol/curve calibration', 'public CMake and new facade APIs/examples/tests',
                                'Python CMake/bindings/tests', 'Excel generated entries/source/tests'],
          'integrated_locally': False, 'latest_master_integrated_gates_validated': False,
          'handoff': 'Coordinator must schedule normal integration and affected validation for the separate PR; specified SHA validation remains scoped to 696edf74.'}
(evidence / 'master-advance.json').write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result, indent=2))
