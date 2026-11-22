"""Recalculate the two-bucket Lookback fixture with the specified Windows Excel XLL."""

import argparse
import json
import platform
from importlib import import_module
from pathlib import Path

import win32com.client


helpers = import_module("run-compounded-ois")
Ref = helpers.Ref


def populate(sheet, high_floor):
    row = 1

    def table(rows):
        nonlocal row
        top = row
        for values in rows:
            for column, value in enumerate(values, 1):
                cell = sheet.Cells(row, column + 16)
                if isinstance(value, Ref):
                    cell.Formula2 = "=" + value
                else:
                    cell.Value2 = value
            row += 1
        address = sheet.Range(sheet.Cells(top, 17), sheet.Cells(row - 1, 16 + len(rows[0]))).Address
        row += 1
        return Ref(address)

    def call(name, *args):
        nonlocal row
        cell = sheet.Cells(row, 17)
        arguments = ",".join(str(arg) if isinstance(arg, Ref) else json.dumps(arg) for arg in args)
        cell.Formula2 = f"={name}({arguments})"
        row += 2
        return Ref(cell.Address)

    def dates(days):
        return call("CALIBRATIONDATES.NEW", table([[day(d)] for d in days]))

    def day(d):
        return helpers.serial(3, d)

    fixings = call("MARKETFIXINGSNAPSHOT.NEW", table([["USD-SOFR"]]), table([[helpers.serial(2, 29)]]), table([[-.012]]))
    valuation = call("SCRIPTVALUATIONSETTINGS.NEW", "risk", table([["evaluation_date", day(1)]]), fixings)
    calendar = call("COMPOUNDEDOISCALENDAR.NEW", table([["center", "explicit"], ["holidays", dates([4])], ["work_weekends", dates([9])]]))
    geometry = call("CURVEINSTRUMENTGEOMETRY.NEW", table([
        ["today", day(1)], ["currency", "USD"], ["knot_dates", dates([1, 5, 20])], ["parameterization", "LOG_DISCOUNT"],
        ["calibrate_discount_curve", False], ["target_overnight_route", "USD.SOFR"]]))
    routes = call("CURVESCRIPTOVERNIGHTROUTES.NEW", table([
        ["id", "projection", "collateral", "tenor", "overnight", "use_target_curve"],
        ["USD.OIS", False, "OIS", "", False, True], ["USD.SOFR", True, "OIS", "", True, False]]))
    market = call("CURVEINSTRUMENTMARKET.NEW", table([["routes", routes]]))
    tables, observations = [], []
    for name, quote in [("short", .030179570014802), ("long", .0362870928259698), ("registered-par", None)]:
        registered = quote is None
        periods = [(1, 6, 6)] if name == "short" else [(1, 6, 6), (6, 11 if registered else 12, 13)]
        fixed = [(1, 11, 15)] if registered else periods
        fixed_leg = call("COMPOUNDEDOISFIXEDLEG.NEW", table([["start", "end", "payment", "accrual"]] +
                         [[day(a), day(b), day(p), (b-a)/365] for a, b, p in fixed]))
        coupons = call("COMPOUNDEDOISCOUPONS.NEW", table([["coupon_id", "start", "end", "payment"]] +
                       [[key, day(a), day(b), day(p)] for key, (a, b, p) in zip(("first", "second"), periods)]))
        convention = call("COMPOUNDEDOISOBSERVATIONCONVENTION.NEW", table(
            [["mode", "Lookback"], ["lookback_business_days", 1]] if registered else [["mode", "None"]]))
        fields = [["instrument_id", name], ["direction", "PayFixed"], ["discount_route", "USD.OIS"],
                  ["projection_route", "USD.SOFR"], ["fixing_index", "USD-SOFR"], ["notional", 100.], ["coupon", .02],
                  ["currency", "USD"], ["collateral_currency", "USD"], ["collateral_type", "OIS"],
                  ["observation_basis", "ACT_360"], ["fixed_basis", "ACT_365F"], ["calendar", calendar],
                  ["fixed_leg", fixed_leg], ["coupons", coupons], ["rule_id", "dal.compounded-ois-cashflows/2"],
                  ["observation_convention", convention]]
        if high_floor and registered:
            fields.append(["min_abs_annuity", 100.])
        spec = call("COMPOUNDEDOISSCRIPTSPEC.NEW", table(fields))
        tables.append(call("COMPOUNDEDOIS.PREPARE", spec, valuation))
        instrument = call("COMPOUNDEDOISSCRIPTINSTRUMENT.NEW", tables[-1])
        if not registered:
            market_quote = call("CALIBRATIONMARKETQUOTE.NEW", name, quote, "Rate")
            observations.append(call("CALIBRATIONOBSERVATION.NEW", name, instrument, "ParRate", market_quote, 1e-13))
    context = call("CURVEINSTRUMENTCALIBRATIONSPEC.NEW", table([
        ["geometry", geometry], ["market", market], ["valuation", valuation],
        ["observations", call("CALIBRATIONOBSERVATIONS.NEW", table([[o] for o in observations]))],
        ["compounded_ois", call("COMPOUNDEDOISPREPARATIONS.NEW", table([[t] for t in tables]))]]))
    options = call("CURVEINSTRUMENTCALIBRATIONOPTIONS.NEW", table([["jacobian_mode", "AAD"]]))
    fit = call("CALIBRATE.CURVEINSTRUMENTS", context, options)
    limits = call("CALIBRATIONNUMBERS.NEW", table([[1e-12], [1e-12]]))
    accuracy = call("SCRIPTCURVEQUOTERISKACCURACY.NEW", table([["residual_absolute_limits", limits],
        ["min_reciprocal_condition", 1e-10], ["response_backward_error_limit", 1e-12], ["relative_pivot_tolerance", 1e-12]]))
    snapshot = call("SCRIPTCURVEQUOTERISKSNAPSHOT.NEW", fit, accuracy)
    ids = call("SCRIPTCURVEQUOTEIDS.NEW", table([["long"], ["short"]]))
    result = call("COMPOUNDEDOISPARRATE.VALUEWITHQUOTERISK", instrument, tables[-1], snapshot, context, ids)
    for address, name in [("A1", "SUMMARY"), ("A7", "SENSITIVITY"), ("A13", "DIAGNOSTICS")]:
        sheet.Range(address).Formula2 = f"=COMPOUNDEDOISPARRATERISKRESULT.GET.{name}({result})"
    sheet.Range("A30").Formula2 = f"=SCRIPTCURVEQUOTERISKSNAPSHOT.GET.SUMMARY({snapshot})"
    return result, [instrument, tables[-1], snapshot, context, ids, fit]


def verify(sheet, high_floor):
    cells = {key: sheet.Range(key).Value2 for key in ("B2", "B3", "B4", "A8", "B8", "C8", "A9", "B9", "C9", "A14", "D14")}
    assert cells["B2"] == ("Unavailable" if high_floor else "Available"), cells
    assert cells["B4"] == sheet.Range("B34").Value2, cells
    assert cells["B4"] and (cells["A8"], cells["B8"], cells["A9"], cells["B9"]) == (1, "long", 0, "short"), cells
    if high_floor:
        assert all(cells[key] in (None, "") for key in ("B3", "C8", "C9")), cells
        assert (cells["A14"], cells["D14"]) == ("InvalidCalibrationAnnuity", "/annuity"), cells
    else:
        for key, value in [("B3", .01856212179263), ("C8", .87138316352446), ("C9", -.27115795540455)]:
            assert abs(cells[key] - value) < (1e-12 if key == "B3" else 1e-10), cells
        assert cells["A14"] is None, cells
    return cells


def run(xll, output):
    excel = win32com.client.DispatchEx("Excel.Application")
    book = None
    try:
        excel.Visible = excel.DisplayAlerts = False
        helpers.require(excel.RegisterXLL(str(xll.resolve())), "XLL registration failed")
        book = excel.Workbooks.Add()
        book.Worksheets(1).Range("A20").Formula2 = "=INIT.GLOBALDATA(1)"
        evidence = dict(windows=platform.version(), excel_version=excel.Version, excel_build=excel.Build,
                        binaries=helpers.loaded_binaries(excel, xll))
        cases = []
        for name, high_floor in [("Lookback", False), ("HighFloor", True)]:
            sheet = book.Worksheets.Add() if high_floor else book.Worksheets(1)
            sheet.Name = name
            cases.append((sheet, populate(sheet, high_floor), high_floor))
        excel.CalculateFullRebuild()
        for sheet, _, high_floor in cases:
            evidence[sheet.Name] = verify(sheet, high_floor)
        book.SaveAs(str(output.with_suffix(".xlsx").resolve()), FileFormat=51)
        excel.Calculation = -4135  # Manual during repository lifetime checks.
        for sheet, (result_address, inputs), _ in cases:
            tags = [sheet.Range(address).Value2 for address in inputs]
            result = sheet.Range(result_address).Value2
            sheet.UsedRange.ClearContents()
            tag_range = sheet.Range(f"A1:A{len(tags)}")
            tag_range.Value2 = tuple((tag,) for tag in tags)
            erased, = excel.Run("REPOSITORY.ERASE", tag_range)
            assert erased == len(tags), erased
            summary = sheet.Range("E1")
            summary.Formula2 = f'=COMPOUNDEDOISPARRATERISKRESULT.GET.SUMMARY("{result}")'
            summary.Calculate()
            identity = sheet.Range("F4").Value2
            assert identity == evidence[sheet.Name]["B4"], identity
            evidence[sheet.Name]["erased_inputs"] = erased
            evidence[sheet.Name]["identity_after_input_release"] = identity
        output.write_text(json.dumps(evidence, indent=2), encoding="utf-8")
    finally:
        if book is not None:
            book.Close(SaveChanges=False)
        excel.Quit()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--xll", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    run(args.xll, args.output)
