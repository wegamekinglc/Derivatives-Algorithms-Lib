

extern "C" __declspec(dllexport) OPER_* xl_EuropeanPdeRiskResult_Get_Prices
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("EuropeanPdeRiskResult_Get_Prices");        
        argName = "result (input #1)";
        const Handle_<StorableEuropeanPdeRiskResult_> result = Excel::ToHandle<StorableEuropeanPdeRiskResult_>(_env, xl_result);
        argName = 0;
		Matrix_<Cell_> prices;
        EuropeanPdeRiskResult_Get_Prices(result, &prices);
        Excel::Retval_ retval;
        retval.Load(prices);
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

struct XlRegister_EuropeanPdeRiskResult_Get_Prices_
{
    XlRegister_EuropeanPdeRiskResult_Get_Prices_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Completed owning result");
        Excel::Register("Base", "xl_EuropeanPdeRiskResult_Get_Prices", "EUROPEANPDERISKRESULT.GET.PRICES", "Copy European call and put prices", "QQ", "result", argHelp, false);
    }
};
static XlRegister_EuropeanPdeRiskResult_Get_Prices_ The_EuropeanPdeRiskResult_Get_Prices_XlRegisterer;
