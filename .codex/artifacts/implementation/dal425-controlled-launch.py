"""Isolated, sealed-source entry for the finite synthetic controls only."""
import os
import resource
import sys

AS_BYTES = 128 * 1024 * 1024
# Limit this fresh isolated interpreter before source loading.
resource.setrlimit(resource.RLIMIT_AS, (AS_BYTES,) * 2)
resource.setrlimit(resource.RLIMIT_CPU, (2, 3))
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
resource.setrlimit(resource.RLIMIT_FSIZE, (1048576,) * 2)
resource.setrlimit(resource.RLIMIT_NOFILE, (32,) * 2)
import fcntl
import importlib.machinery
import importlib.util
from pathlib import Path

if not (sys.flags.isolated and sys.flags.no_site and sys.dont_write_bytecode):
    raise SystemExit('isolated Python -I -S -B required')
if len(sys.argv) != 4 or sys.argv[1] not in ("init", "run") or sys.argv[3] not in ("ok", "nonzero", "burn", "deadline", "flood"):
    raise SystemExit("fixed synthetic init/run only; products/compiler disabled")
source = Path(__file__).with_name("dal425-controlled-execution.py").absolute()
data = source.read_bytes()
if len(data) > 65536:
    raise SystemExit("fixed source byte cap")
fd = os.memfd_create("dal425-supervisor", os.MFD_ALLOW_SEALING)
if os.write(fd, data) != len(data):
    raise SystemExit("source short write")
fcntl.fcntl(fd, fcntl.F_ADD_SEALS, fcntl.F_SEAL_WRITE | fcntl.F_SEAL_GROW | fcntl.F_SEAL_SHRINK | fcntl.F_SEAL_SEAL)
if dict(os.environ) != {"LC_ALL": "C", "TZ": "UTC"}:
    raise SystemExit('fixed environment required')


class SealedLoader(importlib.machinery.SourceFileLoader):
    def get_code(self, fullname):
        frozen = os.pread(fd, 65537, 0)
        if frozen != data:
            raise ValueError('sealed source identity mismatch')
        return compile(frozen, '<dal425-sealed-source>', 'exec', dont_inherit=True, optimize=0)


loader = SealedLoader('dal425_fixed', str(source))
spec = importlib.util.spec_from_loader(loader.name, loader)
module = importlib.util.module_from_spec(spec)
module.__dict__.update(_SOURCE_BYTES=data, _FRESH_ENTRY=True)
sys.modules[loader.name] = module
try:
    loader.exec_module(module)
finally:
    os.close(fd)
module._command(*sys.argv[1:])
