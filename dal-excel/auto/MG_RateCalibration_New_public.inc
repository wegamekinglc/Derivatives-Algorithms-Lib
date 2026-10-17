

extern "C" __declspec(dllexport) OPER_* xl_RateCalibration_New
    (const OPER_* xl_name, const OPER_* xl_source)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("RateCalibration_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "name"; Excel::ValidateRiskRequestText(xl_name, "RateCalibration_New; name");
        argName = "source (input #2)";
        const Handle_<Storable_> source = Excel::ToHandleBase(_env, xl_source);
        argName = 0;
		Handle_<StorableRateCalibrationSnapshot_> calibration;
        RateCalibration_New(name, source, &calibration);
        Excel::Retval_ retval;
        retval.Load(_env, calibration);
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

struct XlRegister_RateCalibration_New_
{
    XlRegister_RateCalibration_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Object name");
        argHelp.push_back("Existing single, joint, staged-XCCY or joint-XCCY result; native default solve options");
        Excel::Register("Base", "xl_RateCalibration_New", "RATECALIBRATION.NEW", "Seal a retained native rate calibration specification and solve anew", "QQQ", "name,source", argHelp, false);
    }
};
static XlRegister_RateCalibration_New_ The_RateCalibration_New_XlRegisterer;
