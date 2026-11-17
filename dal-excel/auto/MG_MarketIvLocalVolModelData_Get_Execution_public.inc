

extern "C" __declspec(dllexport) OPER_* xl_MarketIvLocalVolModelData_Get_Execution
    (const OPER_* xl_product, const OPER_* xl_model, const OPER_* xl_valuation, const OPER_* xl_simulation)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("MarketIvLocalVolModelData_Get_Execution");        
        argName = "product (input #1)";
        const Handle_<ScriptProductData_> product = Excel::ToHandle<ScriptProductData_>(_env, xl_product);
        argName = "model (input #2)";
        const Handle_<MarketIvLocalVolModelData_> model = Excel::ToHandle<MarketIvLocalVolModelData_>(_env, xl_model);
        argName = "valuation (input #3)";
        const Handle_<StorableScriptValuationSettings_> valuation = Excel::ToHandle<StorableScriptValuationSettings_>(_env, xl_valuation, true);
        argName = "simulation (input #4)";
        const Handle_<StorableMonteCarloSettings_> simulation = Excel::ToHandle<StorableMonteCarloSettings_>(_env, xl_simulation, true);
        argName = 0;
		Matrix_<Cell_> table;
        MarketIvLocalVolModelData_Get_Execution(product, model, valuation, simulation, &table);
        Excel::Retval_ retval;
        retval.Load(table);
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

struct XlRegister_MarketIvLocalVolModelData_Get_Execution_
{
    XlRegister_MarketIvLocalVolModelData_Get_Execution_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("product");
        argHelp.push_back("model");
        argHelp.push_back("valuation");
        argHelp.push_back("simulation");
        Excel::Register("Base", "xl_MarketIvLocalVolModelData_Get_Execution", "MARKETIVLOCALVOLMODELDATA.GET.EXECUTION", "Same-source market IV local-volatility model", "QQQQQ", "product,model,[valuation],[simulation]", argHelp, false);
    }
};
static XlRegister_MarketIvLocalVolModelData_Get_Execution_ The_MarketIvLocalVolModelData_Get_Execution_XlRegisterer;
