"""Explicit, one-time A/2 failure disposition and A/3 reservation replacement."""

import json

from a1_budget import Budget, digest
from a1_run import write

CONTROL_SHA = "acf410567786916601584b71a954991b0a2c81f180aab734cd4be0091ed96ec7"
APPROVAL = "01a1e58d-1d44-78ac-9b5e-089ac7de74f0"
A2_MANIFEST_SHA = "c10fb9ef83e2141de1b2443363906db3eacf0571513be124277717230d7a013f"
OLD_CONTROL = "84a6de0aa9c0773e74022a5d21ab865291c15fed37fe913f5b53f496d75e485a"
BOUNDARY = dict(
    identity="1099c383560b6d252b0dd3a6f7d1ec27649e7bbe8637dee64c3cd9de067094f8",
    ledger="14d6151f22c5dfabe483835a6133ccfa998207f7f77e6e80672c6655ff37eda4",
    journal="6bde5a79ce4b2886ca92b2417e8e9ac2018cc64b01459fedc5379a30b6fad771",
    sequence=2284,
)


def replacement_map(context):
    return [
        dict(
            context,
            family=0,
            batch=b,
            path=0,
            old_dimension=8728,
            new_dimension=8736,
            old_start=b * 2**20 * 8728,
            old_end=b * 2**20 * 8728 + 8727,
            new_start=b * 2**20 * 8736,
            new_end=b * 2**20 * 8736 + 8735,
            disposition="RETIRE_UNEXECUTED_REPLACE_SAME_LOGICAL_PREFIX",
        )
        for b in range(16)
    ]


def validate_replacements(rows, context, reserved, completed):
    if reserved != [16, 0] or completed != [0, 0]:
        raise RuntimeError(
            "Only the sixteen unexecuted A/2 reservations can be replaced"
        )
    if rows != replacement_map(context):
        raise RuntimeError("Reservation identity/coordinate mapping mismatch")
    if (
        not context.get("evidence")
        or context["old_identity"] == context["new_identity"]
    ):
        raise RuntimeError("Missing unexecuted evidence or distinct identities")


def verify_boundary(root, expected):
    for key, name in (("ledger", "ledger.json"), ("journal", "ledger.jsonl")):
        path = root / name
        if not path.is_file() or digest(path) != expected[key]:
            raise RuntimeError("Sealed A/2 boundary mismatch: " + name)


def archive_boundary(root, budget):
    target = root / "a3/origin"
    target.mkdir(parents=True, exist_ok=True)
    for path in (budget.path, budget.journal):
        saved = target / path.name
        if saved.exists():
            raise RuntimeError("A/3 boundary already sealed; recovery cannot repeat")
        saved.write_bytes(path.read_bytes())
    for name, target_name in (
        ("manifest.json", "a2-manifest.json"),
        ("F2_linear-alignment.json", "a2-truncated-alignment.json"),
        ("F2_linear-alignment.log", "a2-failure.log"),
    ):
        path = root / "a2" / name
        if path.exists():
            (target / target_name).write_bytes(path.read_bytes())


def recover(root, identity, expected, context, rows):
    verify_boundary(root, expected)
    budget = Budget(root / "ledger.json", expected["identity"])
    if budget.state["sequence"] != expected["sequence"] or budget.state["active_phase"]:
        raise RuntimeError("A/3 recovery requires the exact closed boundary")
    if (
        context["old_identity"] != expected["identity"]
        or context["new_identity"] != identity
    ):
        raise RuntimeError("Wrong execution identity in recovery")
    validate_replacements(
        rows, context, budget.state["paths"], budget.state["paths_completed"]
    )
    if not budget.state.get("a2_stop_reason") or budget.state.get("a3_disposition"):
        raise RuntimeError("Missing true failure or duplicate disposition")
    archive_boundary(root, budget)
    budget.state["a3_disposition"] = dict(
        context=context, replacements=rows, original_stop=budget.state["a2_stop_reason"]
    )
    budget.save("disposition:A/3")
    write(root / "a3/disposition-state.json", budget.state)
    budget.state["identity"] = identity
    budget.state["a3_transition"] = dict(
        expected, new_identity=identity, context=context
    )
    budget.save("transition:A/3")
    write(root / "a3/transition-state.json", budget.state)
    return budget


def validate_recovered(root, manifest, active_identity=None):
    verify_boundary(root / "a3/origin", BOUNDARY)
    budget = Budget(root / "ledger.json", active_identity or manifest["identity"])
    prefix = (root / "a3/origin/ledger.jsonl").read_bytes()
    if not budget.journal.read_bytes().startswith(prefix):
        raise RuntimeError("Original 2284-event prefix changed")
    disposition = budget.state["a3_disposition"]
    context = manifest["context"]
    if (
        disposition["context"] != context
        or disposition["replacements"] != manifest["replacements"]
    ):
        raise RuntimeError("A/3 disposition does not match frozen manifest")
    for filename, event in (
        ("disposition-state.json", "disposition:A/3"),
        ("transition-state.json", "transition:A/3"),
    ):
        path = root / "a3" / filename
        state = json.loads(path.read_text())
        line = budget.journal.read_text().splitlines()[state["sequence"] - 1]
        entry = json.loads(line)
        if entry["event"] != event or entry["state_sha256"] != digest(path):
            raise RuntimeError("A/3 transition snapshot mismatch")
    return budget
