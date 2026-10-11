

extern "C" __declspec(dllexport) OPER_* xl_BlackScholesMonteCarloPlan_New
    (const OPER_* xl_name, const OPER_* xl_product, const OPER_* xl_valuation, const OPER_* xl_smoothing)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("BlackScholesMonteCarloPlan_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "name"; Excel::ValidateRiskRequestText(xl_name, "BlackScholesMonteCarloPlan_New; name"); argName = "product"; Excel::ValidateRiskRequestText(xl_product, "BlackScholesMonteCarloPlan_New; product"); xl_product = Excel::ScriptScalarInput(xl_product); argName = "valuation"; Excel::ValidateRiskRequestText(xl_valuation, "BlackScholesMonteCarloPlan_New; valuation"); xl_valuation = Excel::ScriptScalarInput(xl_valuation); argName = "smoothing"; Excel::ValidateCurvatureNumbers(xl_smoothing, "BlackScholesMonteCarloPlan_New", "smoothing"); xl_smoothing = Excel::ScriptScalarInput(xl_smoothing); const Excel::ScriptSettingsInput_ normalizedSmoothing(xl_smoothing); xl_smoothing = normalizedSmoothing.Get();
        argName = "product (input #2)";
        const Handle_<ScriptProductData_> product = Excel::ToHandle<ScriptProductData_>(_env, xl_product);
        argName = "valuation (input #3)";
        const Handle_<StorableScriptValuationSettings_> valuation = Excel::ToHandle<StorableScriptValuationSettings_>(_env, xl_valuation, true);
        argName = "smoothing (input #4)";
        const double smoothing = Excel::ToDouble(xl_smoothing, true).value_or(0.01);
        argName = 0;
		Handle_<StorableBlackScholesMonteCarloPlan_> plan;
        BlackScholesMonteCarloPlan_New(name, product, valuation, smoothing, &plan);
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

struct XlRegister_BlackScholesMonteCarloPlan_New_
{
    XlRegister_BlackScholesMonteCarloPlan_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Object name");
        argHelp.push_back("Script contract; EXERCISE uses the separate LSMC interface");
        argHelp.push_back("Blank captures native valuation defaults");
        argHelp.push_back("Finite smoothing width; native default 0.01");
        Excel::Register("Base", "xl_BlackScholesMonteCarloPlan_New", "BLACKSCHOLESMONTECARLOPLAN.NEW", "Seal a contract and observations for compiled segmented native AAD", "QQQQQ", "name,product,[valuation],[smoothing]", argHelp, false);
    }
};
static XlRegister_BlackScholesMonteCarloPlan_New_ The_BlackScholesMonteCarloPlan_New_XlRegisterer;
