import argparse
import gzip
import json
import pathlib
import shutil

from batchplan import identity
from fixtures import fixtures
from qualify import digest, write_json
from referenceplan import fingerprint

EXPLICIT = ["N1-0-100-0.03", "N1-1-120-0", "N2-100", "N2-piecewise", "N8-0", "N8-1", "N9a", "N9c"]


def selected(name):
    if name in EXPLICIT or name.startswith(("N3-", "N4-", "N5", "N10", "N11", "N12")):
        return True
    if name.startswith(("N6-", "N7-")):
        return name.split("-")[1] in ["0", "3"] or name == "N7-1-domain-800"
    return False


def product_job(name, request, execution, mode="product"):
    key = fingerprint(dict(name=name, request=request, execution=execution, mode=mode))
    return dict(id=key, mode=mode, request=request, nodes=0)


def reuse_repair(directory, name, request, manifest):
    source = pathlib.Path("build/dal502-repair")
    saved = json.loads((source / (name + ".input.json")).read_text())
    if saved != request:
        raise RuntimeError("repair input does not match frozen fixture")
    lib = pathlib.Path("build/dal502-make/dal-cpp/libdal_cpp.a")
    expected = "7317377c982fd132c20bc8f74634feeb0ad422aa8d1d9dd424deb41077daa125"
    if digest(lib) != expected:
        raise RuntimeError("repair production library identity changed")
    output = source / (name + ".json.gz")
    shutil.copyfile(output, directory / output.name)
    manifest[name] = dict(
        origin="397a26d64b523bec1883eaa13721b3e4b1627b92 repair capture",
        executable=digest(source / "product-driver"),
        library=expected,
        source_file=str(output),
        sha256=digest(output),
    )


def write_workers(directory, jobs, phase):
    paths = []
    for i in range(2):
        path = directory / f"{phase}-{i}.jsonl"
        if path.exists():
            raise RuntimeError("product phase already exists")
        path.write_text("".join(json.dumps(job) + "\n" for job in jobs[i::2]))
        paths.append(str(path.resolve()))
    (directory / "workers.txt").write_text("\n".join(paths) + "\n")


def prepare(directory, executable):
    directory.mkdir(parents=True, exist_ok=True)
    execution = identity(executable)
    jobs = []
    manifest = {}
    names = {}
    for name, request in fixtures().items():
        if selected(name):
            write_json(directory / (name + ".input.json"), request)
            prepare_case(directory, execution, name, request, jobs, manifest, names)
    write_json(directory / "manifest.json", dict(execution=execution, cases=manifest, jobs=names))
    write_workers(directory, jobs, "products")
    print("new product captures", len(jobs), "reused repair captures", len(manifest))


def prepare_case(directory, execution, name, request, jobs, manifest, names):
    if name in ["N7-1-domain-800", "N7-3-domain-800"]:
        reuse_repair(directory, name, request, manifest)
        return
    job = product_job(name, request, execution)
    jobs.append(job)
    names[job["id"]] = name


def collect(directory):
    manifest = json.loads((directory / "manifest.json").read_text())
    for path in directory.glob("*.results.jsonl"):
        collect_file(directory, path, manifest)
    write_json(directory / "manifest.json", manifest)
    print("current product captures", len(manifest["cases"]))


def collect_file(directory, path, manifest):
    with path.open() as stream:
        for line in stream:
            row = json.loads(line)
            name = manifest["jobs"][row["id"]]
            data = dict(row["result"], elapsed_seconds=row["elapsed_seconds"], returncode=0)
            with gzip.open(directory / (name + ".json.gz"), "wt") as target:
                json.dump(data, target, allow_nan=False)
            manifest["cases"][name] = dict(
                origin="new native batch",
                execution=manifest["execution"],
                sha256=digest(directory / (name + ".json.gz")),
                max_rss_kib=row["max_rss_kib"],
                elapsed_seconds=row["elapsed_seconds"],
            )


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("phase", choices=["prepare", "collect"])
    parser.add_argument("--output", required=True, type=pathlib.Path)
    parser.add_argument("--driver", type=pathlib.Path, default=pathlib.Path("build/dal502-tools/dal502_driver"))
    args = parser.parse_args()
    if args.phase == "prepare":
        prepare(args.output, args.driver)
    else:
        collect(args.output)
