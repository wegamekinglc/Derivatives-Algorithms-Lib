"""Verify P2 compounded OIS in a real Windows Excel process and record loaded binaries."""

import argparse
import datetime
import hashlib
import json
import math
from pathlib import Path
import subprocess


ROOT = Path(__file__).resolve().parents[3]


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


class Ref(str):
    pass


class Sheet:
    def __init__(self, excel, sheet):
        self.excel, self.sheet, self.row = excel, sheet, 1

    def table(self, rows):
        width = len(rows[0])
        start, end = self.row, self.row + len(rows) - 1
        address = f"A{start}:{chr(64 + width)}{end}"
        self.sheet.Range(address).Value2 = tuple(tuple(row) for row in rows)
        self.row = end + 3
        return Ref(address)

    def result_block(self, address, rows, columns):
        first = int(address[1:])
        return self.sheet.Range(f"J{first}:{chr(ord('J') + columns - 1)}{first + rows - 1}").Value2

    def call(self, name, *args, error=False):
        def argument(value):
            if isinstance(value, Ref):
                return value
            if isinstance(value, str):
                return '"' + value.replace('"', '""') + '"'
            if value is None:
                return ""
            return str(value)
        address = f"J{self.row}"
        self.row += 25
        self.sheet.Range(address).Formula2 = "=" + name + "(" + ",".join(argument(arg) for arg in args) + ")"
        self.excel.CalculateFullRebuild()
        result = self.sheet.Range(address).Value2
        if not error:
            require(result is not None and not str(result).startswith("#Error:"), f"{name}: {result}")
        return result, Ref(address)


def serial(month, day):
    return (datetime.date(2024, month, day) - datetime.date(1899, 12, 30)).days


def build(sheet, policy, missing=False):
    call = lambda name, *args: sheet.call(name, *args)[0]
    fixed = call("COMPOUNDEDOISFIXEDLEG.NEW", sheet.table([
        ["start", "end", "payment", "accrual"], ["2024-02-29", "2024-03-06", "2024-03-08", 6 / 365]]))
    coupons = call("COMPOUNDEDOISCOUPONS.NEW", sheet.table([
        ["coupon_id", "start", "end", "payment"], ["coupon", "2024-02-29", "2024-03-06", "2024-03-08"]]))
    observations = call("COMPOUNDEDOISOBSERVATIONS.NEW", sheet.table([
        ["coupon_id", "rate_end", "fixing_time", "start", "accrual", "end", "rate_start"],
        ["coupon", "2024-03-01", serial(2, 29), "2024-02-29", 1 / 360, "2024-03-01", "2024-02-29"],
        ["coupon", "2024-03-05", serial(3, 1), "2024-03-01", 4 / 360, "2024-03-05", "2024-03-01"],
        ["coupon", "2024-03-06", serial(3, 5), "2024-03-05", 1 / 360, "2024-03-06", "2024-03-05"]]))
    holidays = call("CALIBRATIONDATES.NEW", sheet.table([["2024-03-04"]]))
    calendar = call("COMPOUNDEDOISCALENDAR.NEW", sheet.table([["center", "explicit"], ["holidays", holidays]]))
    spec = call("COMPOUNDEDOISSCRIPTSPEC.NEW", sheet.table([
        ["instrument_id", "excel-p2"], ["direction", "PayFixed"], ["discount_route", "USD.OIS"],
        ["projection_route", "USD.SOFR"], ["fixing_index", "USD-SOFR"], ["notional", 100.0], ["coupon", 0.02],
        ["currency", "USD"], ["collateral_currency", "USD"], ["collateral_type", "OIS"],
        ["observation_basis", "ACT_360"], ["fixed_basis", "ACT_365F"], ["calendar", calendar],
        ["fixed_leg", fixed], ["coupons", coupons], ["observations", observations]]))
    if missing:
        snapshot = call("MARKETFIXINGSNAPSHOT.NEW", None, None, None)
    else:
        history = sheet.table([["USD-SOFR", serial(2, 29), -0.012],
                               ["USD-SOFR", serial(3, 1), 0.021], ["USD-SOFR", serial(3, 5), 9.0]])
        start = int(history.split(":")[0][1:])
        snapshot = call("MARKETFIXINGSNAPSHOT.NEW", Ref(f"A{start}:A{start+2}"),
                        Ref(f"B{start}:B{start+2}"), Ref(f"C{start}:C{start+2}"))
    valuation = call("SCRIPTVALUATIONSETTINGS.NEW", "p2-valuation",
                     sheet.table([["evaluation_date", serial(3, 1)], ["today_fixing", policy]]), snapshot)
    tables = call("COMPOUNDEDOIS.PREPARE", spec, valuation)
    instrument = call("COMPOUNDEDOISSCRIPTINSTRUMENT.NEW", tables)
    knots = call("CALIBRATIONDATES.NEW", sheet.table([["2024-03-01"], ["2024-03-21"]]))
    geometry = call("CURVEINSTRUMENTGEOMETRY.NEW", sheet.table([
        ["today", "2024-03-01"], ["currency", "USD"], ["knot_dates", knots], ["parameterization", "LOG_DISCOUNT"],
        ["calibrate_discount_curve", False], ["target_overnight_route", "USD.SOFR"]]))
    routes = call("CURVESCRIPTOVERNIGHTROUTES.NEW", sheet.table([
        ["id", "projection", "collateral", "tenor", "overnight", "use_target_curve"],
        ["USD.OIS", False, "OIS", "", False, True], ["USD.SOFR", True, "OIS", "", True, False]]))
    market = call("CURVEINSTRUMENTMARKET.NEW", sheet.table([["routes", routes]]))
    parameters = call("CALIBRATIONNUMBERS.NEW", sheet.table([[-0.002]]))
    request = call("CURVESCRIPTEVALUATIONREQUEST.NEW", sheet.table([
        ["geometry", geometry], ["market", market], ["valuation", valuation], ["parameters", parameters],
        ["compounded_ois", tables], ["derivative_method", "AAD"]]))
    return call("CURVESCRIPTINSTRUMENT.EVALUATE", instrument, request)


def verify(sheet, policy):
    outcome = build(sheet, policy)
    status, _ = sheet.call("CALIBRATIONOUTCOME.GET.STATUS", outcome)
    require(status == "Evaluated", f"evaluation status {status}")
    price, _ = sheet.call("CURVESCRIPTEVALUATION.GET.VALUE", outcome, "Price")
    par, _ = sheet.call("CURVESCRIPTEVALUATION.GET.VALUE", outcome, "ParRate")
    middle = 1 + 4 * 0.021 / 360 if policy == "RequireHistorical" else math.exp(0.0004)
    floating = (1 - 0.012 / 360) * middle * math.exp(0.0001) - 1
    require(abs(price - 100 * (floating - 0.02 * 6 / 365) * math.exp(-0.0007)) <= 1e-10, "Price oracle mismatch")
    require(abs(par - floating / (6 / 365)) <= 1e-10, "ParRate oracle mismatch")
    _, partial_address = sheet.call("CURVESCRIPTEVALUATION.GET.NATIVEPARTIALS", outcome)
    partials = sheet.result_block(partial_address, 2, 1)
    require(all(isinstance(row[0], (int, float)) and math.isfinite(row[0]) for row in partials), "Missing native partials")
    def oracle(theta):
        middle = 1 + 4 * 0.021 / 360 if policy == "RequireHistorical" else math.exp(-theta * 4 / 20)
        floating = (1 - 0.012 / 360) * middle * math.exp(-theta / 20) - 1
        return [100 * (floating - 0.02 * 6 / 365) * math.exp(theta * 7 / 20), floating / (6 / 365)]
    previous = None
    for h in [1e-4, 5e-5]:
        up, down = oracle(-0.002 + h), oracle(-0.002 - h)
        reference = [(a - b) / (2 * h) for a, b in zip(up, down)]
        for i, value in enumerate(reference):
            require(abs(partials[i][0] - value) <= 1e-7 + 1e-5 * abs(value), "Native derivative oracle mismatch")
            if previous is not None:
                require(abs(previous[i] - value) <= 1e-7 + 1e-5 * abs(value), "Unstable FD reference")
        previous = reference
    _, cashflow_address = sheet.call("CURVESCRIPTEVALUATION.GET.TABLE", outcome, "cashflows")
    cashflows = sheet.result_block(cashflow_address, 3, 11)
    require(abs(sum(row[10] for row in cashflows[1:]) - price) <= 1e-10, "Cashflow amounts differ from Price")
    _, observation_address = sheet.call("CURVESCRIPTEVALUATION.GET.TABLE", outcome, "observations")
    observations = sheet.result_block(observation_address, 4, 15)
    require(observations[1][6] == "Historical" and observations[3][6] == "Projection", "Fixing provenance missing")
    require(observations[2][6] == ("Historical" if policy == "RequireHistorical" else "Projection"), "Same-day policy ignored")
    require(observations[2][12:14] == (4 / 360, 4 / 360), "Holiday weights changed")
    return {"policy": policy, "price": price, "par_rate": par, "partials": partials,
            "cashflows": cashflows, "observations": observations}


def loaded_binaries(excel, expected):
    import win32api
    import win32process
    _, pid = win32process.GetWindowThreadProcessId(excel.Hwnd)
    process = win32api.OpenProcess(0x0400 | 0x0010, False, pid)
    try:
        paths = [Path(win32process.GetModuleFileNameEx(process, module))
                 for module in win32process.EnumProcessModules(process)]
    finally:
        process.Close()
    require(any(path.resolve() == expected.resolve() for path in paths), "Requested XLL is not loaded in this Excel process")
    return {"excel_pid": pid, "modules": [{"path": str(path), "sha256": sha256(path)}
            for path in paths if "dal" in path.name.lower() and path.suffix.lower() in [".xll", ".dll"]]}


def run(args):
    import win32com.client
    head = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
    python_evidence = json.loads(args.python_evidence.read_text(encoding="utf-8"))
    require(python_evidence["source_head"] == head, "Installed Python evidence uses another source SHA")
    require(not subprocess.check_output(["git", "status", "--porcelain", "--untracked-files=no"], cwd=ROOT, text=True),
            "Tracked source changes prevent a single-SHA acceptance claim")
    excel = win32com.client.DispatchEx("Excel.Application")
    excel.Visible, excel.DisplayAlerts = False, False
    book = None
    try:
        require(excel.RegisterXLL(str(args.xll.resolve())), "XLL registration failed")
        loaded = loaded_binaries(excel, args.xll)
        book = excel.Workbooks.Add()
        sheet = Sheet(excel, book.Worksheets(1))
        sheet.call("INIT.GLOBALDATA", 1)
        values = [verify(sheet, policy) for policy in ["Model", "RequireHistorical"]]
        failed = build(sheet, "Model", missing=True)
        require(sheet.call("CALIBRATIONOUTCOME.GET.STATUS", failed)[0] == "Failed", "Missing fixing did not fail")
        report = sheet.call("CALIBRATIONOUTCOME.GET.REPORT", failed)[0]
        sheet.sheet.Range("A:I").ClearContents()
        _, error_address = sheet.call("CALIBRATIONREPORT.GET.ERRORS", report)
        code = sheet.result_block(error_address, 2, 1)[1][0]
        require(code == "MissingFixing", "Owning missing-fixing diagnostic was lost")
        for name in ["CURVESCRIPTEVALUATIONREQUEST.NEW", "COMPOUNDEDOISSCRIPTSPEC.NEW"]:
            ref = sheet.table([[None, None], [None, None], ["unknown_after_blank", True]])
            error, _ = sheet.call(name, ref, error=True)
            require("row=3 column=1" in str(error) and "unknown_after_blank" in str(error), "Selected context tail was ignored")
        ref = sheet.table([["coupon_id", "rate_end", "fixing_time", "start", "accrual", "end", "rate_start"],
                           [None] * 7, ["coupon", "2024-03-05", serial(3, 1), "2024-03-01", True, "2024-03-05", "2024-03-01"]])
        error, _ = sheet.call("COMPOUNDEDOISOBSERVATIONS.NEW", ref, error=True)
        require("row=3 column=5" in str(error), "Selected observation tail was ignored")
        args.output.parent.mkdir(parents=True, exist_ok=True)
        workbook = args.output.with_suffix(".xlsx")
        book.SaveAs(str(workbook.resolve()), FileFormat=51)
        evidence = {"source_head": head, "excel_version": excel.Version, "excel_build": excel.Build,
                    "loaded_binaries": loaded, "xll_requested_sha256": sha256(args.xll), "build_cache_sha256": sha256(args.build_cache),
                    "dependencies": subprocess.check_output(["git", "submodule", "status", "--recursive"], cwd=ROOT, text=True),
                    "python_evidence_sha256": sha256(args.python_evidence), "values": values, "missing_fixing_code": code,
                    "tail_error": error, "workbook": workbook.name}
        args.output.write_text(json.dumps(evidence, indent=2), encoding="utf-8")
    finally:
        if book is not None:
            book.Close(SaveChanges=False)
        excel.Quit()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--xll", required=True, type=Path)
    parser.add_argument("--python-evidence", required=True, type=Path)
    parser.add_argument("--build-cache", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    run(parser.parse_args())
