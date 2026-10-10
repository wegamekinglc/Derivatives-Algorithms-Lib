

extern "C" __declspec(dllexport) OPER_* xl_RateCalibration_Get_Parameters
    (const OPER_* xl_calibration)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("RateCalibration_Get_Parameters");        
        argName = "calibration (input #1)";
        const Handle_<StorableRateCalibrationSnapshot_> calibration = Excel::ToHandle<StorableRateCalibrationSnapshot_>(_env, xl_calibration);
        argName = 0;
		Matrix_<Cell_> parameters;
        RateCalibration_Get_Parameters(calibration, &parameters);
        Excel::Retval_ retval;
        retval.Load(parameters);
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

struct XlRegister_RateCalibration_Get_Parameters_
{
    XlRegister_RateCalibration_Get_Parameters_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Owning sealed calibration snapshot");
        Excel::Register("Base", "xl_RateCalibration_Get_Parameters", "RATECALIBRATION.GET.PARAMETERS", "Copy the solved native rate free parameters", "QQ", "calibration", argHelp, false);
    }
};
static XlRegister_RateCalibration_Get_Parameters_ The_RateCalibration_Get_Parameters_XlRegisterer;
