import argparse
import gzip
import json
import pathlib
import shutil

from batchplan import identity
from fixtures import fixtures
from qualify import digest, write_json, load
from referenceplan import fingerprint

EXPLICIT = [
    "N1-0-100-0.03",
    "N1-1-120-0",
    "N2-100",
    "N2-piecewise",
    "N8-0",
    "N8-1",
    "N9a",
    "N9c",
]


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
    (directory / (phase + ".workers.txt")).write_text("\n".join(paths) + "\n")
    if phase == "products":
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
    write_json(
        directory / "manifest.json",
        dict(execution=execution, cases=manifest, jobs=names),
    )
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
            if not line.endswith("\n"):
                break
            row = json.loads(line)
            name = manifest["jobs"][row["id"]]
            if name in manifest["cases"]:
                continue
            data = dict(
                row["result"], elapsed_seconds=row["elapsed_seconds"], returncode=0
            )
            with gzip.open(directory / (name + ".json.gz"), "wt") as target:
                json.dump(data, target, allow_nan=False)
            manifest["cases"][name] = dict(
                origin="new native batch",
                execution=manifest.get("job_execution", {}).get(
                    row["id"], manifest["execution"]
                ),
                sha256=digest(directory / (name + ".json.gz")),
                max_rss_kib=row["max_rss_kib"],
                elapsed_seconds=row["elapsed_seconds"],
            )


def completion_cases(directory, requests):
    return {
        name: request
        for name, request in requests.items()
        if name.startswith(("N1-", "N2-"))
    }


def supplementary_cases(directory, requests):
    cases = {}
    for name, request in requests.items():
        if name.startswith("N2-"):
            cases[name + "-contacts"] = dict(request, audit_contacts=True)
        if name.startswith("N12-"):
            cases[name + "-parallel"] = dict(request, audit_parallel=True)
    return cases


def boundary_cases(directory, requests):
    base = load(directory, "N3-0-100")
    run = next(
        (
            r
            for r in base["base_regions"]
            if r["timeIndex_"] == 0 and not r["rightClipped_"]
        ),
        None,
    )
    if run is None:
        raise RuntimeError("specified positive-intrinsic contact run missing")
    midpoint = (run["lastNode_"] + 0.5) * base["plan"]["gridSpacing_"]
    return {
        f"N10a-{shift}": dict(
            requests["N3-0-100"], spot=midpoint + shift, h=0.5, audit_contacts=True
        )
        for shift in [0, -2, 2]
    }


def more_cases(directory, phase):
    requests = fixtures()
    if phase == "complete":
        return completion_cases(directory, requests), "product"
    if phase == "observed":
        return {"N9a-observed": requests["N9a"]}, "product"
    return (
        dict(
            supplementary_cases(directory, requests),
            **boundary_cases(directory, requests),
        ),
        "audit",
    )


def prepare_more(directory, executable, phase):
    manifest = json.loads((directory / "manifest.json").read_text())
    execution = identity(executable)
    candidates, mode = more_cases(directory, phase)
    existing = set(manifest["cases"]) | set(manifest["jobs"].values())
    jobs = []
    for name, request in candidates.items():
        if name not in existing:
            write_json(directory / (name + ".input.json"), request)
            job = product_job(name, request, execution, mode)
            jobs.append(job)
            manifest["jobs"][job["id"]] = name
            manifest.setdefault("job_execution", {})[job["id"]] = execution
    write_json(directory / "manifest.json", manifest)
    write_workers(directory, jobs, phase)
    print(phase, "new captures", len(jobs))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "phase", choices=["prepare", "collect", "complete", "supplement", "observed"]
    )
    parser.add_argument("--output", required=True, type=pathlib.Path)
    parser.add_argument(
        "--driver",
        type=pathlib.Path,
        default=pathlib.Path("build/dal502-tools/dal502_driver"),
    )
    args = parser.parse_args()
    if args.phase == "prepare":
        prepare(args.output, args.driver)
    elif args.phase == "collect":
        collect(args.output)
    else:
        prepare_more(args.output, args.driver, args.phase)
