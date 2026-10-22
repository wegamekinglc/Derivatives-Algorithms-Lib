"""Collect native commands synchronously; preserve failures and stop dependent work."""

import json
from pathlib import Path
import subprocess
import sys

evidence = Path(__file__).resolve().parent
snapshot = json.loads((evidence / "integration.json").read_text())
root = Path(snapshot["native_root"])
results = []
for name in sys.argv[1:]:
    command = ["cmd.exe", "/d", "/c", snapshot["native_win"].replace("/", "\\") + "\\" + name + ".cmd"]
    result = subprocess.run(command)
    results.append({"name": name, "argv": command, "exit": result.returncode})
    (evidence / ("run-" + sys.argv[1] + ".json")).write_text(json.dumps(results, indent=2) + "\n")
    print(name, result.returncode, flush=True)
    if result.returncode:
        print((root / (name + ".log")).read_text(errors="replace")[-6000:])
        sys.exit(result.returncode)
