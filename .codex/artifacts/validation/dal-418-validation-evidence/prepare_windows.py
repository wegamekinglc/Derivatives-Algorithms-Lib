import hashlib
import io
import json
import pathlib
import subprocess
import tarfile

evidence = pathlib.Path(__file__).resolve().parent
repo = evidence.parents[3]
head = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=repo, text=True).strip()
assert head == "6372ede9452b2ec912234a373666dda9b239a1bc"
root = pathlib.Path("/mnt/d/multica-builds/dal418-eb7d672c679c-6372ede9")
root.mkdir(parents=True, exist_ok=False)
source = root / "source"
source.mkdir()

def extract(cwd, revision, destination):
    data = subprocess.check_output(["git", "archive", revision], cwd=cwd)
    with tarfile.open(fileobj=io.BytesIO(data)) as archive:
        archive.extractall(destination, filter="data")
    return hashlib.sha256(data).hexdigest()

main_hash = extract(repo, head, source)
dependencies = {}
for name in ("eigen", "googletest", "machinist", "pybind11", "rapidjson"):
    relative = "dal-cpp/externals/" + name
    pin = subprocess.check_output(["git", "rev-parse", head + ":" + relative], cwd=repo, text=True).strip()
    dependency = repo / relative
    assert subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=dependency, text=True).strip() == pin
    assert not subprocess.check_output(["git", "status", "--porcelain=v1", "--untracked-files=no"], cwd=dependency)
    dependencies[name] = {"pin": pin, "archive_sha256": extract(dependency, pin, source / relative)}
windows_root = "D:\\multica-builds\\dal418-eb7d672c679c-6372ede9"
driver = "\r\n".join([
    "@echo off", "cd /d \"" + windows_root + "\\source\"",
    "set \"DAL_DIR=" + windows_root + "\\source\"", "set \"NUMBER_OF_PROCESSORS=6\"", "set \"DAL_NUM_THREADS=4\"",
    "call build_windows.bat > \"" + windows_root + "\\windows-test_output.txt\" 2>&1",
    "set \"DAL418_EXIT=%ERRORLEVEL%\"", "echo %DAL418_EXIT% > \"" + windows_root + "\\windows-exit.txt\"",
    "exit /b %DAL418_EXIT%", ""])
(root / "run-validation.cmd").write_bytes(driver.encode("utf-8"))
manifest = {"head": head, "snapshot": str(source), "git_archive_sha256": main_hash,
            "dependencies": dependencies, "windows_driver": windows_root + "\\run-validation.cmd"}
(evidence / "windows-snapshot.json").write_text(json.dumps(manifest, indent=2) + "\n")
print(json.dumps(manifest, indent=2))
