"""Generate the isolated production-entry test translation unit."""

import hashlib
import json
from pathlib import Path
import sys


def declaration(root, path, signature):
    source = (root / path).read_text()
    start = source.index(signature)
    end = source.index("{", start) + 1
    depth = 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


def generate(root, work):
    helpers = [
        ("dal-excel/src/__scriptinput.hpp", "class ScriptSettingsInput_"),
        ("dal-excel/src/__scriptinput.hpp", "inline String_ ScriptSettingLocation("),
        ("dal-excel/src/__scriptinput.hpp", "inline const OPER_* ScriptScalarInput("),
        ("dal-excel/src/__scriptinput.hpp", "inline bool ScriptInputBlank("),
        ("dal-excel/src/__dupireinput.hpp", "inline void ValidateNativeTextInput("),
        ("dal-excel/src/__earlyexercisepdeinput.hpp", "inline bool IsEarlyExercisePdeCell("),
    ]
    raw_header = (root / "dal-excel/src/__earlyexercisepderiskinput.hpp").read_text()
    raw_helpers = raw_header.split("#ifdef _WIN32", 1)[1].split("#endif", 1)[0]
    pieces = [declaration(root, path, signature) + (";" if signature.startswith("class ") else "")
              for path, signature in helpers]
    converter = declaration(root, "dal-excel/src/_excel.cpp", "String_ FromPascalString(wchar_t* src)")
    string_input = declaration(root, "dal-excel/src/_excel.cpp", "String_ Excel::ToString(const OPER_* src, bool optional)")
    typed_handle = declaration(root, "dal-excel/src/_excel.hpp", "template <class T_> Handle_<T_> ToHandle(")
    template = (root / "dal-excel/tests/earlyexerciseinputprobe.in").read_text()
    source = template.replace("@HELPERS@", "\n".join(pieces) + "\n" + typed_handle + "\n" + raw_helpers)
    source = source.replace("@CONVERTER@", converter).replace("@STRING_INPUT@", string_input)
    generated = sorted((root / "dal-excel/auto").glob("MG_EarlyExercisePdeRisk*_public.inc"))
    source = source.replace("@GENERATED@", "\n".join(f'#include "{path}"' for path in generated))
    (work / "probe.cpp").write_text(source)
    (work / "source-identity.json").write_text(json.dumps({
        "source_sha256": hashlib.sha256(source.encode()).hexdigest(),
        "generated_sha256": {path.name: hashlib.sha256(path.read_bytes()).hexdigest() for path in generated},
        "scope": "Production generated bodies and guards; isolated OPER/registry/return adapters, not Windows ABI.",
    }, indent=2) + "\n")


if __name__ == "__main__":
    generate(Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve())
