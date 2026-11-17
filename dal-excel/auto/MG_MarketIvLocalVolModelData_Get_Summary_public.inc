

extern "C" __declspec(dllexport) OPER_* xl_MarketIvLocalVolModelData_Get_Summary
    (const OPER_* xl_model)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("MarketIvLocalVolModelData_Get_Summary");        
        argName = "model (input #1)";
        const Handle_<MarketIvLocalVolModelData_> model = Excel::ToHandle<MarketIvLocalVolModelData_>(_env, xl_model);
        argName = 0;
		Matrix_<Cell_> table;
        MarketIvLocalVolModelData_Get_Summary(model, &table);
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

struct XlRegister_MarketIvLocalVolModelData_Get_Summary_
{
    XlRegister_MarketIvLocalVolModelData_Get_Summary_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("model");
        Excel::Register("Base", "xl_MarketIvLocalVolModelData_Get_Summary", "MARKETIVLOCALVOLMODELDATA.GET.SUMMARY", "Same-source market IV local-volatility model", "QQ", "model", argHelp, false);
    }
};
static XlRegister_MarketIvLocalVolModelData_Get_Summary_ The_MarketIvLocalVolModelData_Get_Summary_XlRegisterer;
