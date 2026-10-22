import json
import pathlib
import subprocess

evidence = pathlib.Path(__file__).resolve().parent
repo = evidence.parents[3]
root = pathlib.Path("/mnt/d/multica-builds/dal418-eb7d672c679c-6372ede9")
records = {}

def add_tree(cwd, ref, prefix=""):
    for row in subprocess.check_output(["git", "ls-tree", "-rz", ref], cwd=cwd).split(b"\0"):
        if not row:
            continue
        metadata, name = row.split(b"\t", 1)
        mode, kind, digest = metadata.split()
        if kind == b"blob":
            records[prefix + name.decode()] = digest.decode()

add_tree(repo, "6372ede9")
for dependency in ("eigen", "googletest", "machinist", "pybind11", "rapidjson"):
    relative = "dal-cpp/externals/" + dependency
    pin = subprocess.check_output(["git", "rev-parse", "6372ede9:" + relative], cwd=repo, text=True).strip()
    add_tree(repo / relative, pin, relative + "/")
(root / "expected-source-blobs.json").write_text(json.dumps(records) + "\n")
verifier = '''import hashlib,json,pathlib
root=pathlib.Path(__file__).resolve().parent
source=root/"source"
expected=json.loads((root/"expected-source-blobs.json").read_text())
different=[]
eol=[]
for name,digest in expected.items():
    data=(source/name).read_bytes()
    actual=hashlib.sha1(b"blob "+str(len(data)).encode()+b"\\0"+data).hexdigest()
    if actual != digest:
        normalized=data.replace(b"\\r\\n",b"\\n")
        actual=hashlib.sha1(b"blob "+str(len(normalized)).encode()+b"\\0"+normalized).hexdigest()
        if actual==digest:
            eol.append(name)
        else:
            different.append(name)
result={"tested_head":"6372ede9452b2ec912234a373666dda9b239a1bc","verified_blobs":len(expected),"eol_exports":eol,"content_mismatches":different}
(root/"windows-source-audit.json").write_text(json.dumps(result,indent=2)+"\\n")
print(json.dumps(result,indent=2))
assert not different,different
'''
(root / "verify_sources.py").write_text(verifier)
print("Prepared native Windows audit for", len(records), "Git blobs")
