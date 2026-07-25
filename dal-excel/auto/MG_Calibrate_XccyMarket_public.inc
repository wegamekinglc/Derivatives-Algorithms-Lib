

extern "C" __declspec(dllexport) OPER_* xl_Calibrate_XccyMarket
    (const OPER_* xl_today, const OPER_* xl_domesticCcy, const OPER_* xl_foreignCcy, const OPER_* xl_domesticBlock, const OPER_* xl_foreignBlock, const OPER_* xl_instruments, const OPER_* xl_knotDates, const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("Calibrate_XccyMarket");        
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
        const Vector_<Handle_<Storable_>> instruments = Excel::ToHandleBaseVector(_env, xl_instruments);
        argName = "knotDates (input #7)";
        const Vector_<Date_> knotDates = Excel::ToDateVector(xl_knotDates);
        argName = "settings (input #8)";
        const Matrix_<Cell_> settings = Excel::ToCellMatrix(xl_settings, true);
      REQUIRE(settings.Cols() == 2 || settings.Empty(), "must have two columns (key, value)");
        argName = 0;
		Handle_<StorableCrossCurrencyCalibrationResult_> result;
        Calibrate_XccyMarket(today, domesticCcy, foreignCcy, domesticBlock, foreignBlock, instruments, knotDates, settings, &result);
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

struct XlRegister_Calibrate_XccyMarket_
{
    XlRegister_Calibrate_XccyMarket_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("The valuation/trade date");
        argHelp.push_back("Domestic currency code (e.g. \"USD\")");
        argHelp.push_back("Foreign currency code (e.g. \"EUR\")");
        argHelp.push_back("The pre-calibrated domestic curve block");
        argHelp.push_back("The pre-calibrated foreign curve block");
        argHelp.push_back("Array of cross-currency swap instrument handles (created with CROSSCURRENCYSWAP.NEW)");
        argHelp.push_back("Knot dates for the basis curve");
        argHelp.push_back("Optional two-column (key,value) settings. Keys: fxSpot, fxForwardCollateral, smoothingWeight, tolerance, fitTolerance, initialGuess, maxEvaluations, maxRestarts, solveMode (EXACT|APPROXIMATE), jacobianMode (ANALYTIC|BUMPED), computeEffJacobianInverse, computeForwardJacobian");
        Excel::Register("Base", "xl_Calibrate_XccyMarket", "CALIBRATE.XCCYMARKET", "Calibrate a cross-currency basis market from instruments and settings. Returns a result handle bundling the basis curve and the fit diagnostics.", "QQQQQQQQQ", "today,domesticCcy,foreignCcy,domesticBlock,foreignBlock,instruments,knotDates,[settings]", argHelp, false);
    }
};
static XlRegister_Calibrate_XccyMarket_ The_Calibrate_XccyMarket_XlRegisterer;
