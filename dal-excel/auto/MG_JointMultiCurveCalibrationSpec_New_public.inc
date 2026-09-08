

extern "C" __declspec(dllexport) OPER_* xl_JointMultiCurveCalibrationSpec_New
    (const OPER_* xl_today, const OPER_* xl_currency, const OPER_* xl_declarations, const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("JointMultiCurveCalibrationSpec_New");        
        argName = "today (input #1)";
        const Date_ today = Excel::ToDate(xl_today);
        argName = "currency (input #2)";
        const String_ currency = Excel::ToString(xl_currency);
        argName = "declarations (input #3)";
        const Vector_<Handle_<Storable_>> declarations = Excel::ToHandleBaseVector(_env, xl_declarations);
        argName = "settings (input #4)";
        const Matrix_<Cell_> settings = Excel::ToCellMatrix(xl_settings, true);
      REQUIRE(settings.Cols() == 2 || settings.Empty(), "must have two columns (key, value)");
        argName = 0;
		Handle_<StorableJointMultiCurveCalibrationSpec_> spec;
        JointMultiCurveCalibrationSpec_New(today, currency, declarations, settings, &spec);
        Excel::Retval_ retval;
        retval.Load(_env, spec);
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

struct XlRegister_JointMultiCurveCalibrationSpec_New_
{
    XlRegister_JointMultiCurveCalibrationSpec_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Calibration date");
        argHelp.push_back("One currency for all declarations");
        argHelp.push_back("Ordered JOINTCURVEDECLARATION.NEW handles");
        argHelp.push_back("Keys: liborBasis, tolerance, fitTolerance, initialGuess, maxEvaluations, maxRestarts, solveMode");
        Excel::Register("Base", "xl_JointMultiCurveCalibrationSpec_New", "JOINTMULTICURVECALIBRATIONSPEC.NEW", "Build a generic same-currency joint calibration spec in declaration order", "QQQQQ", "today,currency,declarations,[settings]", argHelp, false);
    }
};
static XlRegister_JointMultiCurveCalibrationSpec_New_ The_JointMultiCurveCalibrationSpec_New_XlRegisterer;
