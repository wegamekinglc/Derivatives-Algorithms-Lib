

extern "C" __declspec(dllexport) OPER_* xl_RateCalibration_Get_Point
    (const OPER_* xl_calibration)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("RateCalibration_Get_Point");        
        argName = "calibration (input #1)";
        const Handle_<StorableRateCalibrationSnapshot_> calibration = Excel::ToHandle<StorableRateCalibrationSnapshot_>(_env, xl_calibration);
        argName = 0;
		Matrix_<Cell_> point;
        RateCalibration_Get_Point(calibration, &point);
        Excel::Retval_ retval;
        retval.Load(point);
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

struct XlRegister_RateCalibration_Get_Point_
{
    XlRegister_RateCalibration_Get_Point_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Owning sealed calibration snapshot");
        Excel::Register("Base", "xl_RateCalibration_Get_Point", "RATECALIBRATION.GET.POINT", "Copy the complete raw decimal rate quote point", "QQ", "calibration", argHelp, false);
    }
};
static XlRegister_RateCalibration_Get_Point_ The_RateCalibration_Get_Point_XlRegisterer;
