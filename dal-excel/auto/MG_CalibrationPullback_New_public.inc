

extern "C" __declspec(dllexport) OPER_* xl_CalibrationPullback_New
    (const OPER_* xl_name, const OPER_* xl_calibration)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CalibrationPullback_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "name"; Excel::ValidateCalibrationTextInput(xl_name, "CalibrationPullback_New; name");
        argName = "calibration (input #2)";
        const Handle_<Storable_> calibration = Excel::ToHandleBase(_env, xl_calibration);
        argName = 0;
		Handle_<StorableCalibrationPullback_> result;
        CalibrationPullback_New(name, calibration, &result);
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

struct XlRegister_CalibrationPullback_New_
{
    XlRegister_CalibrationPullback_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Object name");
        argHelp.push_back("Frozen Dupire calibration or captured native curve provenance");
        Excel::Register("Base", "xl_CalibrationPullback_New", "CALIBRATIONPULLBACK.NEW", "Create an owning common calibration boundary", "QQQ", "name,calibration", argHelp, false);
    }
};
static XlRegister_CalibrationPullback_New_ The_CalibrationPullback_New_XlRegisterer;
