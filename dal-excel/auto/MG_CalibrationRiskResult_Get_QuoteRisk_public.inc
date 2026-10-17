

extern "C" __declspec(dllexport) OPER_* xl_CalibrationRiskResult_Get_QuoteRisk
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CalibrationRiskResult_Get_QuoteRisk");        
        argName = "result (input #1)";
        const Handle_<StorableCalibrationRiskResult_> result = Excel::ToHandle<StorableCalibrationRiskResult_>(_env, xl_result);
        argName = 0;
		Handle_<StorableCalibrationQuoteRisk_> quoteRisk;
        CalibrationRiskResult_Get_QuoteRisk(result, &quoteRisk);
        Excel::Retval_ retval;
        retval.Load(_env, quoteRisk);
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

struct XlRegister_CalibrationRiskResult_Get_QuoteRisk_
{
    XlRegister_CalibrationRiskResult_Get_QuoteRisk_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Requested quote result");
        Excel::Register("Base", "xl_CalibrationRiskResult_Get_QuoteRisk", "CALIBRATIONRISKRESULT.GET.QUOTERISK", "Copy the full separated native quote contribution result", "QQ", "result", argHelp, false);
    }
};
static XlRegister_CalibrationRiskResult_Get_QuoteRisk_ The_CalibrationRiskResult_Get_QuoteRisk_XlRegisterer;
