

extern "C" __declspec(dllexport) OPER_* xl_Calibrate_SingleCurve
    (const OPER_* xl_today, const OPER_* xl_ccy, const OPER_* xl_instruments, const OPER_* xl_knotDates, const OPER_* xl_settings, const OPER_* xl_discountCurve)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("Calibrate_SingleCurve");        
        argName = "today (input #1)";
        const Date_ today = Excel::ToDate(xl_today);
        argName = "ccy (input #2)";
        const String_ ccy = Excel::ToString(xl_ccy);
        argName = "instruments (input #3)";
        const Vector_<Handle_<Storable_>> instruments = Excel::ToHandleBaseVector(_env, xl_instruments);
        argName = "knotDates (input #4)";
        const Vector_<Date_> knotDates = Excel::ToDateVector(xl_knotDates);
        argName = "settings (input #5)";
        const Matrix_<Cell_> settings = Excel::ToCellMatrix(xl_settings, true);
      REQUIRE(settings.Cols() == 2 || settings.Empty(), "must have two columns (key, value)");
        argName = "discountCurve (input #6)";
        const Handle_<StorableDiscountCurve_> discountCurve = Excel::ToHandle<StorableDiscountCurve_>(_env, xl_discountCurve, true);
        argName = 0;
		Handle_<StorableCurveCalibrationResult_> result;
        Calibrate_SingleCurve(today, ccy, instruments, knotDates, settings, discountCurve, &result);
        Excel::Retval_ retval;
        retval.Load(_env, result);
        return retval.ToXloper();
    }
    catch (std::exception& e)
    {
        return Excel::Error(e.what(), argName);
    }
    catch (...)
    {
        return Excel::Error("Unknown error", argName);
    }
}

struct XlRegister_Calibrate_SingleCurve_
{
    XlRegister_Calibrate_SingleCurve_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("The valuation/trade date");
        argHelp.push_back("Currency code (e.g. \"USD\")");
        argHelp.push_back("Array of instrument handles (created with DEPOSIT.NEW, SWAP.NEW, etc.)");
        argHelp.push_back("Knot dates (can be empty for auto-detection)");
        argHelp.push_back("Optional (key,value) settings. Keys: curveName, calibrateDiscountCurve, solveMode, parameterization, logDfScheme, smoothingWeight, tolerance, fitTolerance, initialGuess, maxEvaluations, maxRestarts, targetCollateral, targetTenor, liborBasis");
        argHelp.push_back("Optional discount curve (OIS-collateralized) needed when calibrating a forward curve (calibrateDiscountCurve=FALSE)");
        Excel::Register("Base", "xl_Calibrate_SingleCurve", "CALIBRATE.SINGLECURVE", "Calibrate a single yield curve from instruments and settings. Returns a result handle bundling the calibrated curve and the fit diagnostics.", "QQQQQQQ", "today,ccy,instruments,knotDates,[settings],[discountCurve]", argHelp, false);
    }
};
static XlRegister_Calibrate_SingleCurve_ The_Calibrate_SingleCurve_XlRegisterer;
