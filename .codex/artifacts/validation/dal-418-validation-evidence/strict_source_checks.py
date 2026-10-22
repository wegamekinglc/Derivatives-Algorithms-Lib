import json
import pathlib
import shlex
import shutil
import subprocess

evidence = pathlib.Path(__file__).resolve().parent
repo = evidence.parents[3]
entries = json.loads((repo / "build/Release-linux/compile_commands.json").read_text())
entry = next(row for row in entries if row["file"].endswith("dal/model/gsrrateoption.cpp"))
base = shlex.split(entry["command"])
output = base.index("-o")
del base[output:output + 2]
base.remove("-c")
warnings = "-Wall -Wextra -Wpedantic -Werror -Wno-unused-parameter -Wno-unused-function -Wno-sign-compare -Wno-dangling-else -Wno-reorder -Wno-switch -Wno-missing-field-initializers -Wno-parentheses -Wno-unused-variable -Wno-redundant-move -Wno-pessimizing-move -Wno-sequence-point -Wno-implicit-fallthrough".split()
results = []
for compiler in ("g++-14", "g++-15"):
    assert shutil.which(compiler), compiler
    argv = [compiler, *base[1:], "-fsyntax-only", *warnings]
    with (evidence / (compiler + "-strict.log")).open("w") as log:
        result = subprocess.run(argv, cwd=entry["directory"], stdout=log, stderr=subprocess.STDOUT)
    results.append({"command": argv, "exit": result.returncode})
    assert result.returncode == 0, compiler
(evidence / "strict-source-checks.json").write_text(json.dumps(results, indent=2) + "\n")
print("GCC14 and GCC15 production-TU warning-as-error checks passed")
