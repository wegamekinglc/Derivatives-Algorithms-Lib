

extern "C" __declspec(dllexport) OPER_* xl_Calibrate_JointXccy
    (const OPER_* xl_valuationTime, const OPER_* xl_currencies, const OPER_* xl_collateralCurrency, const OPER_* xl_fxSpot, const OPER_* xl_domesticInstruments, const OPER_* xl_domesticKnotDates, const OPER_* xl_foreignInstruments, const OPER_* xl_foreignKnotDates, const OPER_* xl_basisInstruments, const OPER_* xl_basisKnotDates, const OPER_* xl_fixings, const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("Calibrate_JointXccy");        
        argName = "valuationTime (input #1)";
        const Cell_ valuationTime = Excel::ToCell(xl_valuationTime);
        argName = "currencies (input #2)";
        const Handle_<StorableCurrencyPair_> currencies = Excel::ToHandle<StorableCurrencyPair_>(_env, xl_currencies);
        argName = "collateralCurrency (input #3)";
        const String_ collateralCurrency = Excel::ToString(xl_collateralCurrency);
        argName = "fxSpot (input #4)";
        const double fxSpot = Excel::ToDouble(xl_fxSpot);
        argName = "domesticInstruments (input #5)";
        const Vector_<Handle_<Storable_>> domesticInstruments = Excel::ToHandleBaseVector(_env, xl_domesticInstruments);
        argName = "domesticKnotDates (input #6)";
        const Vector_<Date_> domesticKnotDates = Excel::ToDateVector(xl_domesticKnotDates);
        argName = "foreignInstruments (input #7)";
        const Vector_<Handle_<Storable_>> foreignInstruments = Excel::ToHandleBaseVector(_env, xl_foreignInstruments);
        argName = "foreignKnotDates (input #8)";
        const Vector_<Date_> foreignKnotDates = Excel::ToDateVector(xl_foreignKnotDates);
        argName = "basisInstruments (input #9)";
        const Vector_<Handle_<Storable_>> basisInstruments = Excel::ToHandleBaseVector(_env, xl_basisInstruments);
        argName = "basisKnotDates (input #10)";
        const Vector_<Date_> basisKnotDates = Excel::ToDateVector(xl_basisKnotDates);
        argName = "fixings (input #11)";
        const Handle_<StorableMarketFixingSnapshot_> fixings = Excel::ToHandle<StorableMarketFixingSnapshot_>(_env, xl_fixings, true);
        argName = "settings (input #12)";
        const Matrix_<Cell_> settings = Excel::ToCellMatrix(xl_settings, true);
      REQUIRE(settings.Cols() == 2 || settings.Empty(), "must have two columns (key, value)");
        argName = 0;
		Handle_<StorableJointXccyCalibrationResult_> result;
        Calibrate_JointXccy(valuationTime, currencies, collateralCurrency, fxSpot, domesticInstruments, domesticKnotDates, foreignInstruments, foreignKnotDates, basisInstruments, basisKnotDates, fixings, settings, &result);
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

struct XlRegister_Calibrate_JointXccy_
{
    XlRegister_Calibrate_JointXccy_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Valuation timestamp as an Excel date-time cell");
        argHelp.push_back("The domestic and foreign currency pair");
        argHelp.push_back("Collateral currency code (currently the domestic currency)");
        argHelp.push_back("Positive domestic-per-foreign FX spot");
        argHelp.push_back("Domestic yield-curve calibration instrument handles");
        argHelp.push_back("Domestic discount-curve knot dates");
        argHelp.push_back("Foreign yield-curve calibration instrument handles");
        argHelp.push_back("Foreign discount-curve knot dates");
        argHelp.push_back("Cross-currency swap instrument handles");
        argHelp.push_back("Cross-currency basis-curve knot dates");
        argHelp.push_back("Immutable fixing snapshot (created with MARKETFIXINGSNAPSHOT.NEW)");
        argHelp.push_back("Optional two-column (key,value) settings. Keys: domesticCurveName, foreignCurveName, basisCurveName, domesticLiborBasis, foreignLiborBasis, domesticParameterization, foreignParameterization, basisParameterization, domesticLogDfScheme, foreignLogDfScheme, domesticSmoothingWeight, foreignSmoothingWeight, basisSmoothingWeight, tolerance, fitTolerance, initialGuess, maxEvaluations, maxRestarts, solveMode, jacobianMode, computeEffJacobianInverse, computeForwardJacobian");
        Excel::Register("Base", "xl_Calibrate_JointXccy", "CALIBRATE.JOINTXCCY", "Jointly calibrate one domestic discount curve, one foreign discount curve, and one cross-currency basis curve", "QQQQQQQQQQQQQ", "valuationTime,currencies,collateralCurrency,fxSpot,domesticInstruments,domesticKnotDates,foreignInstruments,foreignKnotDates,basisInstruments,basisKnotDates,[fixings],[settings]", argHelp, false);
    }
};
static XlRegister_Calibrate_JointXccy_ The_Calibrate_JointXccy_XlRegisterer;
