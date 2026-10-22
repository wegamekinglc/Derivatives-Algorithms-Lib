"""Record actual default consumer, Excel-test and compatibility compile rules."""

import json
from pathlib import Path

evidence = Path(__file__).resolve().parent
snapshot = json.loads((evidence / "integration.json").read_text())
root = Path(snapshot["native_root"])
win = snapshot["native_win"]
prefix = (root / "focused.cmd").read_text().split("cmake -S ", 1)[0]
prefix = prefix.replace("/focused.log", "/audit.log").replace("/focused.exit.txt", "/audit.exit.txt")
commands = []
for version in ("423", "327"):
    for name, path in (("consumer", "consumer-" + version), ("excel", "excel-" + version + "/tests-ON")):
        commands.extend([f"ninja -C {win}/{path} -t commands > {win}/{name}-{version}-commands.txt", "if errorlevel 1 exit /b 1"])
commands.extend([f"ninja -C {win}/focused -t commands > {win}/focused-commands.txt", "if errorlevel 1 exit /b 1", "exit /b 0", ""])
(root / "audit.cmd").write_bytes((prefix + "\n".join(commands)).replace("\n", "\r\n").encode())
