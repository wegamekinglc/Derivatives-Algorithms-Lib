

extern "C" __declspec(dllexport) OPER_* xl_GSRSLVQuoteRiskResult_Get_Calibration
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("GSRSLVQuoteRiskResult_Get_Calibration");        
        argName = "result (input #1)";
        const Handle_<StorableGSRSLVQuoteRiskResult_> result = Excel::ToHandle<StorableGSRSLVQuoteRiskResult_>(_env, xl_result);
        argName = 0;
		Handle_<StorableGSRSLVCalibrationResult_> calibration;
        GSRSLVQuoteRiskResult_Get_Calibration(result, &calibration);
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

struct XlRegister_GSRSLVQuoteRiskResult_Get_Calibration_
{
    XlRegister_GSRSLVQuoteRiskResult_Get_Calibration_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Quote risk result");
        Excel::Register("Base", "xl_GSRSLVQuoteRiskResult_Get_Calibration", "GSRSLVQUOTERISKRESULT.GET.CALIBRATION", "Extract the base SLV calibration from quote risk", "QQ", "result", argHelp, false);
    }
};
static XlRegister_GSRSLVQuoteRiskResult_Get_Calibration_ The_GSRSLVQuoteRiskResult_Get_Calibration_XlRegisterer;
