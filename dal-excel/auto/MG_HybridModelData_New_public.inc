

extern "C" __declspec(dllexport) OPER_* xl_HybridModelData_New
    (const OPER_* xl_name, const OPER_* xl_domesticCurrency, const OPER_* xl_components, const OPER_* xl_correlation)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("HybridModelData_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "domesticCurrency (input #2)";
        const String_ domesticCurrency = Excel::ToString(xl_domesticCurrency);
        argName = "components (input #3)";
        const Vector_<Handle_<Storable_>> components = Excel::ToHandleBaseVector(_env, xl_components);
        argName = "correlation (input #4)";
        const Handle_<HybridCorrelationData_> correlation = Excel::ToHandle<HybridCorrelationData_>(_env, xl_correlation);
        argName = 0;
		Handle_<ModelData_> model;
        HybridModelData_New(name, domesticCurrency, components, correlation, &model);
        Excel::Retval_ retval;
        retval.Load(_env, model);
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

struct XlRegister_HybridModelData_New_
{
    XlRegister_HybridModelData_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Model name");
        argHelp.push_back("Domestic currency");
        argHelp.push_back("Equity and rate component handles");
        argHelp.push_back("Named factor correlation handle");
        Excel::Register("Base", "xl_HybridModelData_New", "HYBRIDMODELDATA.NEW", "Compose named equity and deterministic-rate components", "QQQQQ", "name,domesticCurrency,components,correlation", argHelp, false);
    }
};
static XlRegister_HybridModelData_New_ The_HybridModelData_New_XlRegisterer;
