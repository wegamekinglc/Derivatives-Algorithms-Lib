import codecs
import hashlib
import json
from pathlib import Path
import re
import subprocess

repo = Path(__file__).resolve().parents[4]
relative = "dal-cpp/dal/script/visitor/debugger.hpp"
baseline = subprocess.check_output(["git", "show", "origin/master:" + relative], cwd=repo).decode()
fixed = (repo / relative).read_text()
literal = re.compile(r'"(?:\\.|[^"\\])*"')
before = literal.findall(baseline)
after = literal.findall(fixed)
assert len(before) == len(after)
changed = []
for index, (left, right) in enumerate(zip(before, after)):
    left_bytes = codecs.escape_decode(left[1:-1].encode())[0]
    right_bytes = codecs.escape_decode(right[1:-1].encode())[0]
    assert left_bytes == right_bytes, (index, left, right)
    if left != right:
        changed.append({"literal_index": index, "utf8_hex": left_bytes.hex()})
def nonliteral_tokens(source):
    without_literals = literal.sub('"literal"', source)
    without_comments = re.sub(r"//[^\n]*|/\*.*?\*/", "", without_literals, flags=re.S)
    return re.sub(r"\s+", "", without_comments)


assert nonliteral_tokens(baseline) == nonliteral_tokens(fixed)
assert changed
result = {"all_literal_bytes_equal": True, "nonliteral_tokens_equal": True, "literal_count": len(before), "changed": changed,
          "baseline_sha256": hashlib.sha256(baseline.encode()).hexdigest(), "fixed_sha256": hashlib.sha256(fixed.encode()).hexdigest()}
Path(__file__).with_name("literal-audit.json").write_text(json.dumps(result, indent=2) + "\n")
print(json.dumps(result, indent=2))
