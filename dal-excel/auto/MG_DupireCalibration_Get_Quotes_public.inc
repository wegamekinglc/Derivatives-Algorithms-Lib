

extern "C" __declspec(dllexport) OPER_* xl_DupireCalibration_Get_Quotes
    (const OPER_* xl_calibration)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("DupireCalibration_Get_Quotes");        
        argName = "calibration (input #1)";
        const Handle_<StorableDupireCalibration_> calibration = Excel::ToHandle<StorableDupireCalibration_>(_env, xl_calibration);
        argName = 0;
		Matrix_<Cell_> quotes;
        DupireCalibration_Get_Quotes(calibration, &quotes);
        Excel::Retval_ retval;
        retval.Load(quotes);
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

struct XlRegister_DupireCalibration_Get_Quotes_
{
    XlRegister_DupireCalibration_Get_Quotes_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Frozen calibration");
        Excel::Register("Base", "xl_DupireCalibration_Get_Quotes", "DUPIRECALIBRATION.GET.QUOTES", "Extract frozen spread quote coordinates and values", "QQ", "calibration", argHelp, false);
    }
};
static XlRegister_DupireCalibration_Get_Quotes_ The_DupireCalibration_Get_Quotes_XlRegisterer;
