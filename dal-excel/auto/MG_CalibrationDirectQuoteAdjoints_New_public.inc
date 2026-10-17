

extern "C" __declspec(dllexport) OPER_* xl_CalibrationDirectQuoteAdjoints_New
    (const OPER_* xl_name, const OPER_* xl_calibration, const OPER_* xl_adjoints)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CalibrationDirectQuoteAdjoints_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "name"; Excel::ValidateCalibrationTextInput(xl_name, "CalibrationDirectQuoteAdjoints_New; name");
        argName = "calibration (input #2)";
        const Handle_<StorableCalibrationPullback_> calibration = Excel::ToHandle<StorableCalibrationPullback_>(_env, xl_calibration);
        argName = "adjoints"; Excel::ValidateCalibrationAdjointsInput(xl_adjoints, "CalibrationDirectQuoteAdjoints_New; adjoints", calibration->val_.QuoteRows(), calibration->val_.QuoteCols()); const Excel::ScriptSettingsInput_ adjointsInput(xl_adjoints); xl_adjoints = adjointsInput.Get();
        argName = "adjoints (input #3)";
        const Matrix_<double> adjoints = Excel::ToDoubleMatrix(xl_adjoints);
        argName = 0;
		Handle_<StorableCalibrationDirectQuoteAdjoints_> result;
        CalibrationDirectQuoteAdjoints_New(name, calibration, adjoints, &result);
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

struct XlRegister_CalibrationDirectQuoteAdjoints_New_
{
    XlRegister_CalibrationDirectQuoteAdjoints_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Object name");
        argHelp.push_back("Common boundary");
        argHelp.push_back("Finite direct PV derivatives in the canonical quote matrix");
        Excel::Register("Base", "xl_CalibrationDirectQuoteAdjoints_New", "CALIBRATIONDIRECTQUOTEADJOINTS.NEW", "Copy direct quote adjoints into an owning common seed", "QQQQ", "name,calibration,adjoints", argHelp, false);
    }
};
static XlRegister_CalibrationDirectQuoteAdjoints_New_ The_CalibrationDirectQuoteAdjoints_New_XlRegisterer;
