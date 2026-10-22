

extern "C" __declspec(dllexport) OPER_* xl_CalibrationObservations_New
    (const OPER_* xl_observations)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CalibrationObservations_New");        
        argName = "observations (input #1)";
        const Vector_<Handle_<Storable_>> observations = Excel::ToHandleBaseVector(_env, xl_observations);
        argName = 0;
		Handle_<StorableCalibrationObservations_> value;
        CalibrationObservations_New(observations, &value);
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

struct XlRegister_CalibrationObservations_New_
{
    XlRegister_CalibrationObservations_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Typed observations");
        Excel::Register("Base", "xl_CalibrationObservations_New", "CALIBRATIONOBSERVATIONS.NEW", "Owning typed script calibration interface", "QQ", "observations", argHelp, false);
    }
};
static XlRegister_CalibrationObservations_New_ The_CalibrationObservations_New_XlRegisterer;
