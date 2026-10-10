

extern "C" __declspec(dllexport) OPER_* xl_RateTradeQuoteCurvatureResult_Get_BaseCalibration
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("RateTradeQuoteCurvatureResult_Get_BaseCalibration");        
        argName = "result (input #1)";
        const Handle_<StorableRateTradeQuoteCurvatureResult_> result = Excel::ToHandle<StorableRateTradeQuoteCurvatureResult_>(_env, xl_result);
        argName = 0;
		Handle_<StorableRateCalibrationSnapshot_> calibration;
        RateTradeQuoteCurvatureResult_Get_BaseCalibration(result, &calibration);
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

struct XlRegister_RateTradeQuoteCurvatureResult_Get_BaseCalibration_
{
    XlRegister_RateTradeQuoteCurvatureResult_Get_BaseCalibration_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Completed owning native rate result");
        Excel::Register("Base", "xl_RateTradeQuoteCurvatureResult_Get_BaseCalibration", "RATETRADEQUOTECURVATURERESULT.GET.BASECALIBRATION", "Copy owning base snapshot; its quote plan supplies complete raw metadata", "QQ", "result", argHelp, false);
    }
};
static XlRegister_RateTradeQuoteCurvatureResult_Get_BaseCalibration_ The_RateTradeQuoteCurvatureResult_Get_BaseCalibration_XlRegisterer;
