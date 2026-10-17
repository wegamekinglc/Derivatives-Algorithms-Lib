

extern "C" __declspec(dllexport) OPER_* xl_RateTradeQuoteCurvatureResult_Get_Value
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("RateTradeQuoteCurvatureResult_Get_Value");        
        argName = "result (input #1)";
        const Handle_<StorableRateTradeQuoteCurvatureResult_> result = Excel::ToHandle<StorableRateTradeQuoteCurvatureResult_>(_env, xl_result);
        argName = 0;
		double value;
        RateTradeQuoteCurvatureResult_Get_Value(result, &value);
        Excel::Retval_ retval;
        retval.Load(value);
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

struct XlRegister_RateTradeQuoteCurvatureResult_Get_Value_
{
    XlRegister_RateTradeQuoteCurvatureResult_Get_Value_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Completed owning native rate result");
        Excel::Register("Base", "xl_RateTradeQuoteCurvatureResult_Get_Value", "RATETRADEQUOTECURVATURERESULT.GET.VALUE", "Copy weighted native portfolio pv", "QQ", "result", argHelp, false);
    }
};
static XlRegister_RateTradeQuoteCurvatureResult_Get_Value_ The_RateTradeQuoteCurvatureResult_Get_Value_XlRegisterer;
