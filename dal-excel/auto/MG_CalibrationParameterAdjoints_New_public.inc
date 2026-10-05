

extern "C" __declspec(dllexport) OPER_* xl_CalibrationParameterAdjoints_New
    (const OPER_* xl_name, const OPER_* xl_calibration, const OPER_* xl_adjoints)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CalibrationParameterAdjoints_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "name"; Excel::ValidateCalibrationTextInput(xl_name, "CalibrationParameterAdjoints_New; name");
        argName = "calibration (input #2)";
        const Handle_<StorableCalibrationPullback_> calibration = Excel::ToHandle<StorableCalibrationPullback_>(_env, xl_calibration);
        argName = "adjoints"; Excel::ValidateCalibrationAdjointsInput(xl_adjoints, "CalibrationParameterAdjoints_New; adjoints", calibration->val_.ParameterRows(), calibration->val_.ParameterCols()); const Excel::ScriptSettingsInput_ adjointsInput(xl_adjoints); xl_adjoints = adjointsInput.Get();
        argName = "adjoints (input #3)";
        const Matrix_<double> adjoints = Excel::ToDoubleMatrix(xl_adjoints);
        argName = 0;
		Handle_<StorableCalibrationParameterAdjoints_> result;
        CalibrationParameterAdjoints_New(name, calibration, adjoints, &result);
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

struct XlRegister_CalibrationParameterAdjoints_New_
{
    XlRegister_CalibrationParameterAdjoints_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Object name");
        argHelp.push_back("Common boundary");
        argHelp.push_back("Finite raw PV derivatives in the canonical parameter matrix");
        Excel::Register("Base", "xl_CalibrationParameterAdjoints_New", "CALIBRATIONPARAMETERADJOINTS.NEW", "Copy canonical parameter adjoints into an owning common seed", "QQQQ", "name,calibration,adjoints", argHelp, false);
    }
};
static XlRegister_CalibrationParameterAdjoints_New_ The_CalibrationParameterAdjoints_New_XlRegisterer;
