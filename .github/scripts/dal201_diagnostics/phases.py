"""Resolve verified ELF boundaries and collect separately labeled perturbation data."""

import json
from pathlib import Path
import re
import struct

from diagnostics import (ABI, HERE, clean_environment, require_process, run_process,
                         sha256, verify_report, worker_command, write_json)

HARVEST = "_ZN3Dal20HarvestCurveJacobianERNS_3AAD5Tape_ERNS_7Vector_INS0_7Number_EEES6_RKNS3_IiEE"
REGISTER = "_ZN3Dal23RegisterCurveParametersERKNS_7Vector_IdEE"
RTTI = b"*N3Dal12_GLOBAL__N_126JointXccyResidualFunction_E\0"


def unique(values, description):
    values = list(values)
    if len(values) != 1:
        raise ValueError(f"ambiguous/missing {description}: {values}")
    return values[0]


def resolve_gradient(data, relocation_text, frame_text, symbol_text):
    if data[:6] != b"\x7fELF\x02\x01" or struct.unpack_from("<H", data, 18)[0] != 62:
        raise ValueError("require little-endian x86-64 ELF")
    phoff = struct.unpack_from("<Q", data, 32)[0]
    entsize, count = struct.unpack_from("<HH", data, 54)
    segments = [struct.unpack_from("<IIQQQQQQ", data, phoff + index * entsize) for index in range(count)]
    position = unique((match.start() for match in re.finditer(re.escape(RTTI), data)), "Gradient RTTI name")
    name_address = unique((virtual + position - offset for kind, flags, offset, virtual, physical, size, memory, alignment in segments
                           if kind == 1 and offset <= position < offset + size), "RTTI load segment")
    relocations = {int(match[1], 16): int(match[2], 16) for match in re.finditer(
        r"^([0-9a-f]+)\s+[0-9a-f]+\s+R_X86_64_RELATIVE\s+([0-9a-f]+)", relocation_text, re.M)}
    typeinfo = unique((offset - 8 for offset, target in relocations.items() if target == name_address), "typeinfo relocation")
    vtable = unique((offset - 8 for offset, target in relocations.items() if target == typeinfo), "vtable relocation")
    slot = vtable + 0x38
    gradient = relocations[slot]
    residual = relocations[vtable + 0x30]
    ranges = [(int(match[1], 16), int(match[2], 16)) for match in re.finditer(r"pc=([0-9a-f]+)\.\.([0-9a-f]+)", frame_text)]
    def extent(address):
        end = unique((end for start, end in ranges if start == address and end > start), "exact function FDE")
        unique((virtual for kind, flags, offset, virtual, physical, size, memory, alignment in segments
                if kind == 1 and flags & 1 and virtual <= address < end <= virtual + size), "executable function segment")
        return {"start": address, "end": end}
    symbols = {match[4]: (int(match[1], 16), int(match[2], 16)) for match in re.finditer(
        r"^([0-9a-f]+)\s+([0-9a-f]+)\s+(\w)\s+(\S+)$", symbol_text, re.M)}
    anchor = {"symbol": HARVEST, **extent(symbols[HARVEST][0])}
    return {"typeinfo": typeinfo, "vtable": vtable, "gradient_slot": slot,
            "gradient": extent(gradient), "double_residual": extent(residual), "anchor": anchor,
            "register": {"symbol": REGISTER, **extent(symbols[REGISTER][0])},
            "payoff_root": {"status": "unresolved", "reason": "No separately named PayoffRoot symbol in archived dynamic symbol table; do not guess an inlined address.",
                            "symbol_matches": [name for name in symbols if "PayoffRoot" in name]}}


def resolve(binary, output):
    texts = {}
    for label, command in (
        ("relocations", ["readelf", "-Wr", str(binary)]),
        ("frames", ["readelf", "--debug-dump=frames", str(binary)]),
        ("symbols", ["nm", "-DS", "--defined-only", str(binary)]),
        ("disassembly", ["objdump", "-dC", str(binary)]),
    ):
        require_process(run_process(command, output / label, timeout=60))
        if label != "disassembly":
            texts[label] = (output / label / "stdout.txt").read_text()
    result = resolve_gradient(binary.read_bytes(), texts["relocations"], texts["frames"], texts["symbols"])
    result["binary_sha256"] = sha256(binary)
    write_json(output / "resolution.json", result)
    return output / "resolution.json"


def compile_probe(source, output):
    library = output / "phase-probe.so"
    command = ["g++-14", "-std=c++17", "-O2", "-fPIC", "-shared", "-pthread", "-I" + str(source / "dal-cpp"),
               str(HERE / "phase_probe.cpp"), "-o", str(library), "-ldl",
               "-Wl,--version-script=" + str(HERE / "probe.map")]
    require_process(run_process(command, output / "probe-build", timeout=120))
    write_json(output / "probe-identity.json", {"sha256": sha256(library), "source_sha256": sha256(HERE / "phase_probe.cpp")})
    return library


def verify_phases(directory):
    records = [json.loads(line) for line in (directory / "phases.jsonl").read_text().splitlines()]
    for tag, harvests in ((2, 3), (3, 1)):
        selected = [record for record in records if record["tag"] == tag]
        totals = [record for record in selected if record["phase"] == "target_inclusive"]
        if len(totals) != 4:
            raise ValueError("missing validation/warmup/measured target invocation")
        for total in totals:
            invocation = [record for record in selected if record["invocation"] == total["invocation"]]
            for phase in ("gradient_inclusive", "register", "recording_to_harvest", "harvest"):
                if len([record for record in invocation if record["phase"] == phase]) != harvests:
                    raise ValueError(f"phase hook missed or duplicated {phase}")
            for record in invocation:
                if record["phase"] == "harvest" and (record["rows"], record["cols"], record["nodes"]) != (25, 25, 2511):
                    raise ValueError("changed ANALYTIC work fingerprint")
                if record["ns"] <= 0:
                    raise ValueError("nonpositive phase duration")
    return records


def summarize_phases(records):
    summaries = []
    for total in (row for row in records if row["phase"] == "target_inclusive"):
        rows = [row for row in records if (row["tag"], row["invocation"]) == (total["tag"], total["invocation"])]
        costs = {phase: sum(row["ns"] for row in rows if row["phase"] == phase) for phase in (
            "gradient_inclusive", "register", "recording_to_harvest", "harvest", "double_residual")}
        for parent in (row for row in rows if row["phase"] == "gradient_inclusive"):
            children = sum(row["ns"] for row in rows if row["phase"] in ("register", "recording_to_harvest", "harvest")
                           and row["gradient"] == parent["gradient"])
            if children > parent["ns"]:
                raise ValueError("child exceeds Gradient phase boundary")
        remainder = total["ns"] - costs["gradient_inclusive"] - costs["double_residual"]
        if remainder < 0:
            raise ValueError("children exceed target phase boundary")
        summaries.append({"tag": total["tag"], "invocation": total["invocation"], "target_ns": total["ns"],
                          "costs_ns": costs, "unattributed_remainder_ns": remainder})
    return summaries


def prepare_phases(packages, source, output):
    perf = run_process(["perf", "record", "-e", "cpu-clock:u", "--call-graph", "dwarf,8192",
                        "-o", str(output / "perf-preflight.data"), "--", "true"], output / "perf-preflight", timeout=10)
    write_json(output / "tool-preflight.json", {"perf": perf, "fallback": "verified native boundary hooks"})
    library = compile_probe(source, output)
    resolutions = {side: resolve(packages / side / "dal" / f"_dal.{ABI}.so", output / f"elf-{side}") for side in ("base", "head")}
    return library, resolutions, perf


def collect_phases(packages, source, output, prepared):
    library, resolutions, perf = prepared
    fingerprints = None
    for index, sides in enumerate((("base", "head"), ("head", "base")), 1):
        for side in sides:
            destination = output / "instrumented" / f"{index:02d}-{side}"
            command = worker_command(packages / side, source, destination / "benchmark", "phases")
            command += ["--probe", str(library), "--resolution", str(resolutions[side])]
            env = clean_environment()
            env.update(LD_PRELOAD=str(library), DAL201_BINARY=str(packages / side / "dal" / f"_dal.{ABI}.so"))
            require_process(run_process(command, destination, timeout=180, env=env, cwd=output))
            verify_report(destination / "benchmark/results.json", side)
            rows = verify_phases(destination / "benchmark")
            write_json(destination / "phase-summary.json", summarize_phases(rows))
            audit = json.loads((destination / "benchmark/solver-audit.json").read_text())
            if len(audit) != 8 or any(row["solver_evaluations"] != 8 or not row["converged"] for row in audit):
                raise ValueError("solver workload fingerprint changed")
            work = [{key: row[key] for key in ("tag", "invocation", "gradient", "rows", "cols", "nodes", "inputs", "residuals", "jacobian")}
                    for row in rows if row["phase"] == "harvest"]
            if fingerprints is not None and work != fingerprints:
                raise ValueError("base/head instrumented work fingerprints differ")
            fingerprints = work
    write_json(output / "phase-coverage.json", {
        "status": "partial", "root": "unresolved_inlined_boundary",
        "recording": "NewRecording return to Harvest entry; includes active Residuals and call overhead",
        "harvest": "exported function inclusive; hashing outside timer",
        "gradient": "RTTI/vtable slot + exact FDE; inclusive of probe/hashing overhead",
        "solver_assembly": "target total minus Gradient and double F durations; includes Python/native conversion and remaining solver work",
        "warning": "Perturbed observations only. Root cost requires reviewed instruction ownership of the saved disassembly/perf samples; no inferred root nanoseconds.",
    })


def collect_perf(packages, source, output, probe):
    availability = {"status": probe["status"], "fallback": "verified native boundary hooks", "samples_hz": 997}
    if probe["status"] == "passed":
        for side in ("base", "head"):
            destination = output / "perf" / side
            command = ["perf", "record", "-F", "997", "-e", "cpu-clock:u", "--call-graph", "dwarf,8192",
                       "-o", str(output / f"perf-{side}.data"), "--"]
            command += worker_command(packages / side, source, destination / "benchmark")
            result = run_process(command, destination, timeout=180, env=clean_environment(), cwd=output)
            availability[side] = result["status"]
            write_json(output / "perf-availability.json", availability)
            require_process(result)
            verify_report(destination / "benchmark/results.json", side)
            if result["status"] == "passed":
                run_process(["perf", "script", "-i", str(output / f"perf-{side}.data")],
                            output / f"perf-script-{side}", timeout=60)
    write_json(output / "perf-availability.json", availability)
