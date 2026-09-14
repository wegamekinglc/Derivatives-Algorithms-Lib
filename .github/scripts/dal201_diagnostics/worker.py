"""Run the unchanged suite prefix against exactly one archived package."""

import argparse
import ctypes
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from diagnostics import ABI, TARGETS, load_lock, verify_hash, write_json


def prefix(cases, expected):
    end = next(index for index, case in enumerate(cases) if case.name == TARGETS[-1])
    selected = cases[:end + 1]
    if [case.name for case in selected] != expected:
        raise ValueError("suite prefix changed or reordered")
    return selected


def tag_workload(work, tag, marker, factory):
    def run():
        marker(tag)
        try:
            return work.run()
        finally:
            marker(0)
    return factory(run, work.validate)


def loaded_package(package):
    sys.path.insert(0, str(package))
    import dal
    expected = package / "dal" / f"_dal.{ABI}.so"
    if Path(dal._dal.__file__).resolve() != expected.resolve():
        raise ValueError("native import escaped selected package or wrong ABI")
    if Path(dal.__file__).resolve() != (package / "dal/__init__.py").resolve():
        raise ValueError("Python wrapper import escaped selected package")
    return dal


def instrument(cases, output, probe, resolution):
    from dal_benchmarks.harness import Workload
    import dal
    verify_hash(Path(dal._dal.__file__), resolution["binary_sha256"])
    native = ctypes.CDLL(dal._dal.__file__)
    library = ctypes.CDLL(str(probe))
    library.Dal201Install.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_size_t, ctypes.c_size_t, ctypes.c_size_t]
    library.Dal201Install.restype = ctypes.c_int
    anchor = getattr(native, resolution["anchor"]["symbol"])
    address = ctypes.cast(anchor, ctypes.c_void_p).value
    bias = address - resolution["anchor"]["start"]
    if library.Dal201Install(address, resolution["anchor"]["start"], resolution["gradient_slot"],
                            resolution["gradient"]["start"], resolution["double_residual"]["start"]):
        raise ValueError("verified Gradient vtable hook installation failed")
    write_json(output / "runtime-map.json", {"load_bias": bias, "resolution": resolution})
    library.Dal201Tag.argtypes = [ctypes.c_int]
    library.Dal201Dump.argtypes = [ctypes.c_char_p]
    library.Dal201Dump.restype = ctypes.c_int
    audit = []
    for tag, name in enumerate(TARGETS, 1):
        case = next(case for case in cases if case.name == name)
        prepare = case.prepare
        def tagged_prepare(prepare=prepare, tag=tag, name=name):
            work = prepare()
            def validate(value):
                work.validate(value)
                if name.startswith("xccy."):
                    matrix = value.jacobian_at_solution
                    audit.append({"case": name, "solver_evaluations": value.solver_evaluations,
                                  "converged": value.converged, "residuals": list(value.residuals),
                                  "rows": matrix.rows(), "cols": matrix.cols()})
            return tag_workload(Workload(work.run, validate), tag, library.Dal201Tag, Workload)
        case.prepare = tagged_prepare
    def finish():
        write_json(output / "solver-audit.json", audit)
        if library.Dal201Dump(str(output / "phases.jsonl").encode()):
            raise ValueError("phase hooks missed calls, overflowed, or failed to write evidence")
    return finish


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package", type=Path, required=True)
    parser.add_argument("--suite", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--import-only", action="store_true")
    parser.add_argument("--mode", choices=("plain", "phases"), default="plain")
    parser.add_argument("--probe", type=Path)
    parser.add_argument("--resolution", type=Path)
    args = parser.parse_args()
    loaded_package(args.package.resolve())
    if args.import_only:
        print(json.dumps({"import": "passed", "abi": ABI}), flush=True)
        return 0
    args.output.mkdir(parents=True, exist_ok=False)
    sys.path.insert(0, str(args.suite.resolve()))
    from dal_benchmarks.cases import build_cases
    from dal_benchmarks.runner import parser as suite_parser, run_cases
    cases = prefix(build_cases(False), load_lock()["prefix"])
    finish = None
    if args.mode == "phases":
        finish = instrument(cases, args.output, args.probe, json.loads(args.resolution.read_text()))
    options = suite_parser().parse_args(["--samples", "1", "--warmups", "2", "--output-dir", str(args.output)])
    try:
        return run_cases(cases, options)
    finally:
        if finish:
            finish()


if __name__ == "__main__":
    raise SystemExit(main())
