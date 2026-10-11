

extern "C" __declspec(dllexport) OPER_* xl_BlackScholesLsmcPlan_New
    (const OPER_* xl_name, const OPER_* xl_product, const OPER_* xl_valuation, const OPER_* xl_simulation)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("BlackScholesLsmcPlan_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "name"; Excel::ValidateRiskRequestText(xl_name, "BlackScholesLsmcPlan_New; name"); argName = "product"; Excel::ValidateRiskRequestText(xl_product, "BlackScholesLsmcPlan_New; product"); xl_product = Excel::ScriptScalarInput(xl_product); argName = "valuation"; Excel::ValidateRiskRequestText(xl_valuation, "BlackScholesLsmcPlan_New; valuation"); xl_valuation = Excel::ScriptScalarInput(xl_valuation); argName = "simulation"; Excel::ValidateRiskRequestText(xl_simulation, "BlackScholesLsmcPlan_New; simulation"); xl_simulation = Excel::ScriptScalarInput(xl_simulation);
        argName = "product (input #2)";
        const Handle_<ScriptProductData_> product = Excel::ToHandle<ScriptProductData_>(_env, xl_product);
        argName = "valuation (input #3)";
        const Handle_<StorableScriptValuationSettings_> valuation = Excel::ToHandle<StorableScriptValuationSettings_>(_env, xl_valuation, true);
        argName = "simulation (input #4)";
        const Handle_<StorableMonteCarloSettings_> simulation = Excel::ToHandle<StorableMonteCarloSettings_>(_env, xl_simulation, true);
        argName = 0;
		Handle_<StorableBlackScholesLsmcPlan_> plan;
        BlackScholesLsmcPlan_New(name, product, valuation, simulation, &plan);
        Excel::Retval_ retval;
        retval.Load(_env, plan);
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

struct XlRegister_BlackScholesLsmcPlan_New_
{
    XlRegister_BlackScholesLsmcPlan_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Object name");
        argHelp.push_back("Script contract with strictly future EXERCISE dates");
        argHelp.push_back("Blank captures native valuation defaults");
        argHelp.push_back("Blank selects native risk defaults with AAD enabled");
        Excel::Register("Base", "xl_BlackScholesLsmcPlan_New", "BLACKSCHOLESLSMCPLAN.NEW", "Seal native AAD LSMC preparation with copied observations and settings", "QQQQQ", "name,product,[valuation],[simulation]", argHelp, false);
    }
};
static XlRegister_BlackScholesLsmcPlan_New_ The_BlackScholesLsmcPlan_New_XlRegisterer;
