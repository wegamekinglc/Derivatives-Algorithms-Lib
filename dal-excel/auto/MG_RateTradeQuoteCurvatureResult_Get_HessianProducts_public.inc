

extern "C" __declspec(dllexport) OPER_* xl_RateTradeQuoteCurvatureResult_Get_HessianProducts
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("RateTradeQuoteCurvatureResult_Get_HessianProducts");        
        argName = "result (input #1)";
        const Handle_<StorableRateTradeQuoteCurvatureResult_> result = Excel::ToHandle<StorableRateTradeQuoteCurvatureResult_>(_env, xl_result);
        argName = 0;
		Matrix_<Cell_> products;
        RateTradeQuoteCurvatureResult_Get_HessianProducts(result, &products);
        Excel::Retval_ retval;
        retval.Load(products);
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

struct XlRegister_RateTradeQuoteCurvatureResult_Get_HessianProducts_
{
    XlRegister_RateTradeQuoteCurvatureResult_Get_HessianProducts_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Completed owning native rate result");
        Excel::Register("Base", "xl_RateTradeQuoteCurvatureResult_Get_HessianProducts", "RATETRADEQUOTECURVATURERESULT.GET.HESSIANPRODUCTS", "Copy direction rows and full quote columns; zero directions spill one blank cell", "QQ", "result", argHelp, false);
    }
};
static XlRegister_RateTradeQuoteCurvatureResult_Get_HessianProducts_ The_RateTradeQuoteCurvatureResult_Get_HessianProducts_XlRegisterer;
