

extern "C" __declspec(dllexport) OPER_* xl_CalibrateXccyMarket
    (const OPER_* xl_today, const OPER_* xl_domesticCcy, const OPER_* xl_foreignCcy, const OPER_* xl_domesticBlock, const OPER_* xl_foreignBlock, const OPER_* xl_instruments, const OPER_* xl_knotDates, const OPER_* xl_settings)
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {
        Logging::Write("CalibrateXccyMarket");
        argName = "today (input #1)";
        const Date_ today = Excel::ToDate(xl_today);
        argName = "domesticCcy (input #2)";
        const String_ domesticCcy = Excel::ToString(xl_domesticCcy);
        argName = "foreignCcy (input #3)";
        const String_ foreignCcy = Excel::ToString(xl_foreignCcy);
        argName = "domesticBlock (input #4)";
        const Handle_<StorableCurveBlock_> domesticBlock = Excel::ToHandle<StorableCurveBlock_>(_env, xl_domesticBlock);
        argName = "foreignBlock (input #5)";
        const Handle_<StorableCurveBlock_> foreignBlock = Excel::ToHandle<StorableCurveBlock_>(_env, xl_foreignBlock);
        argName = "instruments (input #6)";
        const Vector_<Handle_<StorableCrossCurrencySwap_>> instruments = Excel::ToHandleVector<StorableCrossCurrencySwap_>(_env, xl_instruments);
        argName = "knotDates (input #7)";
        const Vector_<Date_> knotDates = Excel::ToDateVector(xl_knotDates, true);
        argName = "settings (input #8)";
        const Dictionary_ settings = Excel::ToDictionary(xl_settings, true);
        argName = 0;
		Handle_<StorableDiscountCurve_> basisCurve;
		double maxAbsResidual;
		double rmsResidual;
        CalibrateXccyMarket(today, domesticCcy, foreignCcy, domesticBlock, foreignBlock, instruments, knotDates, settings, &basisCurve, &maxAbsResidual, &rmsResidual);
        Excel::Retval_ retval;
        retval.Load(_env, basisCurve);
        retval.Load(maxAbsResidual);
        retval.Load(rmsResidual);
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

struct XlRegister_CalibrateXccyMarket_
{
    XlRegister_CalibrateXccyMarket_()
    {
        Vector_<String_> argHelp;
        argHelp.push_back("The valuation/trade date");
        argHelp.push_back("Domestic currency code (e.g. \"USD\")");
        argHelp.push_back("Foreign currency code (e.g. \"EUR\")");
        argHelp.push_back("The pre-calibrated domestic curve block");
        argHelp.push_back("The pre-calibrated foreign curve block");
        argHelp.push_back("Array of cross-currency swap instrument handles");
        argHelp.push_back("Knot dates for the basis curve");
        argHelp.push_back("Optional settings as a two-column range");
        Excel::Register("Base", "xl_CalibrateXccyMarket", "DA.CALIBRATE.XCCY.MARKET", "Calibrate a cross-currency basis market from instruments and settings", "QQQQQQQQQ!", "today,domesticCcy,foreignCcy,domesticBlock,foreignBlock,instruments,knotDates,settings", argHelp, false);
    }
};
static XlRegister_CalibrateXccyMarket_ The_CalibrateXccyMarket_XlRegisterer;
