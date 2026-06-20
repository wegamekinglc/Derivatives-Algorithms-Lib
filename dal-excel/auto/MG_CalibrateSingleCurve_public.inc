

extern "C" __declspec(dllexport) OPER_* xl_CalibrateSingleCurve
    (const OPER_* xl_today, const OPER_* xl_ccy, const OPER_* xl_instruments, const OPER_* xl_knotDates, const OPER_* xl_settings)
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {
        Logging::Write("CalibrateSingleCurve");
        argName = "today (input #1)";
        const Date_ today = Excel::ToDate(xl_today);
        argName = "ccy (input #2)";
        const String_ ccy = Excel::ToString(xl_ccy);
        argName = "instruments (input #3)";
        const Vector_<Handle_<StorableYCInstrument_>> instruments = Excel::ToHandleVector<StorableYCInstrument_>(_env, xl_instruments);
        argName = "knotDates (input #4)";
        const Vector_<Date_> knotDates = Excel::ToDateVector(xl_knotDates, true);
        argName = "settings (input #5)";
        const Dictionary_ settings = Excel::ToDictionary(xl_settings, true);
        argName = 0;
		Handle_<StorableDiscountCurve_> curve;
		Vector_<> marketRates;
		Vector_<> modelRates;
		Vector_<> residuals;
		double maxAbsResidual;
		double rmsResidual;
        CalibrateSingleCurve(today, ccy, instruments, knotDates, settings, &curve, &marketRates, &modelRates, &residuals, &maxAbsResidual, &rmsResidual);
        Excel::Retval_ retval;
        retval.Load(_env, curve);
        retval.Load(maxAbsResidual);
        retval.Load(rmsResidual);
        retval.Load(marketRates);
        retval.Load(modelRates);
        retval.Load(residuals);
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

struct XlRegister_CalibrateSingleCurve_
{
    XlRegister_CalibrateSingleCurve_()
    {
        Vector_<String_> argHelp;
        argHelp.push_back("The valuation/trade date");
        argHelp.push_back("Currency code (e.g. \"USD\")");
        argHelp.push_back("Array of instrument handles (created with DA.DEPOSIT.NEW, DA.SWAP.NEW, etc.)");
        argHelp.push_back("Knot dates (can be empty for auto-detection)");
        argHelp.push_back("Optional settings as a two-column range");
        Excel::Register("Base", "xl_CalibrateSingleCurve", "DA.CALIBRATE.SINGLE.CURVE", "Calibrate a single yield curve from instruments and settings", "QQQQQQ!", "today,ccy,instruments,knotDates,settings", argHelp, false);
    }
};
static XlRegister_CalibrateSingleCurve_ The_CalibrateSingleCurve_XlRegisterer;
