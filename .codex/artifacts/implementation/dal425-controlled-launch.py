"""Fresh, sealed-source entry for the finite synthetic controls only."""
import os
import resource
import sys

AS_BYTES = 128 * 1024 * 1024
# Before source hashing/imports; exec then replaces the launcher's mappings.
resource.setrlimit(resource.RLIMIT_AS, (AS_BYTES,) * 2)
resource.setrlimit(resource.RLIMIT_CPU, (2, 3))
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
resource.setrlimit(resource.RLIMIT_FSIZE, (1048576,) * 2)
resource.setrlimit(resource.RLIMIT_NOFILE, (32,) * 2)
import fcntl
from pathlib import Path

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
os.set_inheritable(fd, True)
bootstrap = """import os, sys
fd = int(sys.argv[1])
data = os.pread(fd, 65537, 0)
os.close(fd)
g = {"__name__":"dal425_fixed", "__file__":sys.argv[2], "_SOURCE_BYTES":data, "_FRESH_ENTRY":True}
exec(compile(data, "<dal425-sealed-source>", "exec"), g)
g["_command"](*sys.argv[3:])
"""
os.execve(sys.executable, [sys.executable, "-I", "-S", "-B", "-c", bootstrap, str(fd), str(source), *sys.argv[1:]], {"LC_ALL":"C", "TZ":"UTC"})
