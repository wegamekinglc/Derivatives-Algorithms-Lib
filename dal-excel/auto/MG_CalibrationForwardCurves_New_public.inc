

extern "C" __declspec(dllexport) OPER_* xl_CalibrationForwardCurves_New
    (const OPER_* xl_bindings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CalibrationForwardCurves_New");        
        argName = "bindings (input #1)";
        const Matrix_<Cell_> bindings = Excel::ToCellMatrix(xl_bindings, false, true);
        const Excel::ScriptSettingsInput_ normalized_bindings(xl_bindings); xl_bindings = normalized_bindings.Get();
        argName = 0;
		Handle_<StorableCalibrationForwardCurves_> value;
        CalibrationForwardCurves_New(bindings, &value);
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

struct XlRegister_CalibrationForwardCurves_New_
{
    XlRegister_CalibrationForwardCurves_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Typed bindings");
        Excel::Register("Base", "xl_CalibrationForwardCurves_New", "CALIBRATIONFORWARDCURVES.NEW", "Owning typed script calibration interface", "QQ", "bindings", argHelp, false);
    }
};
static XlRegister_CalibrationForwardCurves_New_ The_CalibrationForwardCurves_New_XlRegisterer;
