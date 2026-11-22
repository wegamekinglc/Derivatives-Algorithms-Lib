

extern "C" __declspec(dllexport) OPER_* xl_CurveCalibrationStage_New
    (const OPER_* xl_today, const OPER_* xl_currency, const OPER_* xl_instruments, const OPER_* xl_knotDates, const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CurveCalibrationStage_New");        
        argName = "today (input #1)";
        const Date_ today = Excel::ToDate(xl_today);
        argName = "currency (input #2)";
        const String_ currency = Excel::ToString(xl_currency);
        argName = "instruments (input #3)";
        const Vector_<Handle_<Storable_>> instruments = Excel::ToHandleBaseVector(_env, xl_instruments);
        argName = "knotDates (input #4)";
        const Vector_<Date_> knotDates = Excel::ToDateVector(xl_knotDates, true);
        argName = "settings (input #5)";
        const Matrix_<Cell_> settings = Excel::ToCellMatrix(xl_settings, true);
      REQUIRE(settings.Cols() == 2 || settings.Empty(), "must have two columns (key, value)");
        argName = 0;
		Handle_<StorableCurveCalibrationSpec_> stage;
        CurveCalibrationStage_New(today, currency, instruments, knotDates, settings, &stage);
        Excel::Retval_ retval;
        retval.Load(_env, stage);
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

struct XlRegister_CurveCalibrationStage_New_
{
    XlRegister_CurveCalibrationStage_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Calibration date");
        argHelp.push_back("Currency of the stage");
        argHelp.push_back("Market instruments in original quote order");
        argHelp.push_back("Explicit knots. When omitted the stage uses instrument maturities.");
        argHelp.push_back("Stage settings as in CALIBRATE.SINGLECURVE, including curveName, calibrateDiscountCurve, targetTenor, parameterization and tolerance");
        Excel::Register("Base", "xl_CurveCalibrationStage_New", "CURVECALIBRATIONSTAGE.NEW", "Define one stage of a sequential same-currency curve calibration", "QQQQQQ", "today,currency,instruments,[knotDates],[settings]", argHelp, false);
    }
};
static XlRegister_CurveCalibrationStage_New_ The_CurveCalibrationStage_New_XlRegisterer;
