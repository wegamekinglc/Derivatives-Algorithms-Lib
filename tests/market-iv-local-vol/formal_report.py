"""Frozen N3/N5 price and paired acceptance gates."""


def statistical_gate(estimate, se, oracle, strict):
    if se / 100 > 1e-4:
        return "INCOMPLETE"
    limit = 5e-5 if strict else 6e-4
    return "PASS" if abs(estimate - oracle) / 100 <= limit + 4.5 * se / 100 else "FAIL"


def gate(estimate, se, oracle, strict, deterministic, reference_status):
    if estimate is None or se is None:
        return "NOT_RUN"
    if deterministic:
        return (
            "PASS" if abs(estimate - oracle) / max(1, abs(oracle)) <= 2e-12 else "FAIL"
        )
    if reference_status != "PASS":
        return "REFERENCE_UNAVAILABLE"
    return statistical_gate(estimate, se, oracle, strict)


def qualified_status(price, grid, pair, deterministic, strict):
    if price != "PASS" or deterministic or strict:
        return price
    if pair != "PASS":
        return pair
    if grid["status"] == "PASS":
        return "PASS"
    certificate = grid.get("reference_self_check")
    return (
        "FAIL"
        if certificate and certificate["status"] == "PASS"
        else "REFERENCE_UNAVAILABLE"
    )


def paired_gate(value, se):
    if value is None or se is None:
        return "NOT_RUN"
    if se / 100 > 5e-5:
        return "INCOMPLETE"
    return "PASS" if abs(value) / 100 <= 2.5e-4 + 4.5 * se / 100 else "FAIL"
