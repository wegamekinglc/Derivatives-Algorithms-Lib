

extern "C" __declspec(dllexport) OPER_* xl_PortfolioRiskResult_Get_Product
    (const OPER_* xl_result, const OPER_* xl_trade)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("PortfolioRiskResult_Get_Product");        
        argName = "result (input #1)";
        const Handle_<Storable_> result = Excel::ToHandle<Storable_>(_env, xl_result);
        argName = "trade"; Excel::ValidatePortfolioTradeOrdinal(xl_trade, "PortfolioRiskResult_Get_Product");
        argName = "trade (input #2)";
        const double trade = Excel::ToDouble(xl_trade);
        argName = 0;
		Matrix_<Cell_> product;
        PortfolioRiskResult_Get_Product(result, trade, &product);
        Excel::Retval_ retval;
        retval.Load(product);
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

struct XlRegister_PortfolioRiskResult_Get_Product_
{
    XlRegister_PortfolioRiskResult_Get_Product_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Completed weighted or attribution portfolio result");
        argHelp.push_back("Strict zero-based original trade ordinal");
        Excel::Register("Base", "xl_PortfolioRiskResult_Get_Product", "PORTFOLIORISKRESULT.GET.PRODUCT", "Extract one original trade's frozen source table", "QQQ", "result,trade", argHelp, false);
    }
};
static XlRegister_PortfolioRiskResult_Get_Product_ The_PortfolioRiskResult_Get_Product_XlRegisterer;
