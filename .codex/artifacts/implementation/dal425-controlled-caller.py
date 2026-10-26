"""Private fixed-host allocation and receipt binding; no execution gate."""
import hashlib
import json
import os
from pathlib import Path
import stat

GRANT_SHA256 = "4250347a85508d07830dc92ab7de98a025f7fa3a6239bea098a0f3385ebbfecd"
REVISION = "dal425-execution-ledger-after-s0k/r1"


def collect_reaped(raw, persist, drain):
    persist(raw)
    try:
        return drain()
    except BlockingIOError:
        return None


def validate_allocation(grant, ledger, guard, task, owner):
    required = {"schema": 1, "kind": "actual_coordinator_engineering_allocation",
                "ledger_revision": REVISION, "attempt_serial": 1, "attempts_max": 1,
                "status": "DEBITED_HELD_NO_REFUND_BEFORE_DISPATCH", "owner_agent_id": owner,
                "reserved_wall_seconds": 60, "reserved_cpu_core_seconds": 60,
                "product_evaluations_allowed": 0, "workers_allowed": 0,
                "new_stage_byte_subcap": 8388608, "new_stage_file_subcap": 256}
    if (not isinstance(grant, dict) or any(type(grant.get(k)) is not type(v) or grant[k] != v for k, v in required.items())
            or not task or not owner):
        raise ValueError("shared allocation missing or invalid")
    entry = ledger.get("next_S0L_actual_caller_binding", {})
    if entry.get("grant") != grant or entry.get("grant_sha256") != GRANT_SHA256:
        raise ValueError("shared allocation ledger mismatch")
    if (guard.get("filter_task_id") != task or guard.get("filter_agent_id") != owner
            or guard.get("source_task_id") != grant.get("issuer_task_id") or guard.get("enabled") is not True):
        raise ValueError("shared allocation exact task/guard mismatch")
    return grant


def _encoded(value):
    return json.dumps(value, sort_keys=True, allow_nan=False).encode()


def _exclusive(path, data):
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600)
    try:
        if os.write(fd, data) != len(data):
            raise OSError("short write; allocation remains consumed")
        os.fsync(fd)
    finally:
        os.close(fd)
    directory = os.open(path.parent, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    try:
        os.fsync(directory)
    finally:
        os.close(directory)


class HeldAttempt:
    """Attach once to an already consumed real grant in one trusted private root."""
    def __init__(self, root, anchor, grant_bytes, ledger_bytes, guard_bytes, task, owner, identities, slots):
        grant = json.loads(grant_bytes)
        if hashlib.sha256(grant_bytes).hexdigest() != GRANT_SHA256:
            raise ValueError("shared allocation hash mismatch")
        validate_allocation(grant, json.loads(ledger_bytes), json.loads(guard_bytes), task, owner)
        self.root = Path(root).resolve()
        info = self.root.lstat()
        if not stat.S_ISDIR(info.st_mode) or info.st_uid != os.getuid() or info.st_mode & 0o077:
            raise ValueError("private caller root required")
        claim = json.loads(Path(anchor).read_bytes())
        expected = {"grant_sha256": GRANT_SHA256, "allocation_id": grant["allocation_id"],
                    "ledger_revision": REVISION, "task_id": task, "owner": owner,
                    "attempt_serial": 1, "root": str(self.root), "state": "PENDING_NO_REFUND"}
        if any(claim.get(k) != v for k, v in expected.items()):
            raise ValueError("shared allocation consumed root/attempt mismatch")
        if (set(identities) != {"source", "observer", "profile", "argv", "env", "cwd"}
                or not all(identities[k] for k in identities)):
            raise ValueError("fixed caller identities required")
        if (type(slots) is not dict or "caller.pending" not in slots
                or any(type(v) is not int or v <= 0 or Path(k).name != k for k, v in slots.items())
                or sum(slots.values()) > grant["new_stage_byte_subcap"] or len(slots) > grant["new_stage_file_subcap"]):
            raise ValueError("fixed output predebit invalid")
        self.slots = dict(slots)
        self.spent = set()
        self.failed = False
        binding = {**expected, "ledger_sha256": hashlib.sha256(ledger_bytes).hexdigest(),
                   "guard_sha256": hashlib.sha256(guard_bytes).hexdigest(), "identities": identities,
                   "slots": slots, "held_bytes": sum(slots.values()), "held_files": len(slots),
                   "successor": False, "allocation_refund": False}
        self.write("caller.pending", _encoded(binding))

    def write(self, name, data):
        if self.failed or name not in self.slots or name in self.spent or len(data) > self.slots[name]:
            self.failed = True
            raise ValueError("caller write predebit exhausted; no successor")
        self.spent.add(name)
        try:
            _exclusive(self.root / name, data)
        except BaseException:
            self.failed = True
            raise

    def record_wait4(self, raw, drain):
        return collect_reaped(raw, lambda value: self.write("wait4.json", _encoded(value)), drain)
