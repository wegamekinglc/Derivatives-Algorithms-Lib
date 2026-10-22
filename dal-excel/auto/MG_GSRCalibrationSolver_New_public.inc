

extern "C" __declspec(dllexport) OPER_* xl_GSRCalibrationSolver_New
    (const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("GSRCalibrationSolver_New");        
        argName = "settings (input #1)";
        const Matrix_<Cell_> settings = Excel::ToCellMatrix(xl_settings);
        const Excel::ScriptSettingsInput_ normalized_settings(xl_settings); xl_settings = normalized_settings.Get(); argName = "settings"; Excel::ValidateScriptSettingsRange(xl_settings, "GSRCalibrationSolver_New", "settings");
        argName = 0;
		Handle_<StorableGSRCalibrationSolver_> value;
        GSRCalibrationSolver_New(settings, &value);
        Excel::Retval_ retval;
        retval.Load(_env, value);
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

struct XlRegister_GSRCalibrationSolver_New_
{
    XlRegister_GSRCalibrationSolver_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Exact snake_case field/value rows; pricing is an optional typed handle");
        Excel::Register("Base", "xl_GSRCalibrationSolver_New", "GSRCALIBRATIONSOLVER.NEW", "Wrap native selected-g optimizer settings without changing legacy defaults", "QQ", "settings", argHelp, false);
    }
};
static XlRegister_GSRCalibrationSolver_New_ The_GSRCalibrationSolver_New_XlRegisterer;
