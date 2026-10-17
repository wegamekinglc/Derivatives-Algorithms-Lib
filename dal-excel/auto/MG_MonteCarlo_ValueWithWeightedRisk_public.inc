

extern "C" __declspec(dllexport) OPER_* xl_MonteCarlo_ValueWithWeightedRisk
    (const OPER_* xl_product, const OPER_* xl_modelData, const OPER_* xl_n_paths, const OPER_* xl_request, const OPER_* xl_valuation, const OPER_* xl_simulation)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("MonteCarlo_ValueWithWeightedRisk");        
        argName = "product (input #1)";
        const Handle_<ScriptProductData_> product = Excel::ToHandle<ScriptProductData_>(_env, xl_product);
        xl_product = Excel::ScriptScalarInput(xl_product);
        argName = "modelData (input #2)";
        const Handle_<ModelData_> modelData = Excel::ToHandle<ModelData_>(_env, xl_modelData);
        xl_modelData = Excel::ScriptScalarInput(xl_modelData); argName = "n_paths"; Excel::ValidateRiskRequestPaths(xl_n_paths, "MonteCarlo_ValueWithWeightedRisk");
        argName = "n_paths (input #3)";
        const double n_paths = Excel::ToDouble(xl_n_paths);
        xl_request = Excel::ScriptScalarInput(xl_request); xl_valuation = Excel::ScriptScalarInput(xl_valuation); xl_simulation = Excel::ScriptScalarInput(xl_simulation);
        argName = "request (input #4)";
        const Handle_<StorableWeightedRiskRequest_> request = Excel::ToHandle<StorableWeightedRiskRequest_>(_env, xl_request, true);
        argName = "valuation (input #5)";
        const Handle_<StorableScriptValuationSettings_> valuation = Excel::ToHandle<StorableScriptValuationSettings_>(_env, xl_valuation, true);
        argName = "simulation (input #6)";
        const Handle_<StorableMonteCarloSettings_> simulation = Excel::ToHandle<StorableMonteCarloSettings_>(_env, xl_simulation, true);
        argName = 0;
		Handle_<StorableWeightedRiskResult_> result;
        MonteCarlo_ValueWithWeightedRisk(product, modelData, n_paths, request, valuation, simulation, &result);
        Excel::Retval_ retval;
        retval.Load(_env, result);
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

struct XlRegister_MonteCarlo_ValueWithWeightedRisk_
{
    XlRegister_MonteCarlo_ValueWithWeightedRisk_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Product handle");
        argHelp.push_back("Model data handle");
        argHelp.push_back("Positive integral paths per replicate, at most INT_MAX");
        argHelp.push_back("Blank selects payoff with unit weight and all native inputs");
        argHelp.push_back("Blank captures current date and required history");
        argHelp.push_back("Blank enables native AAD; explicit enable_aad=false requests price only");
        Excel::Register("Base", "xl_MonteCarlo_ValueWithWeightedRisk", "MONTECARLO.VALUEWITHWEIGHTEDRISK", "Value one fixed-weight objective and retain its native gradient", "QQQQQQQ", "product,modelData,n_paths,[request],[valuation],[simulation]", argHelp, false);
    }
};
static XlRegister_MonteCarlo_ValueWithWeightedRisk_ The_MonteCarlo_ValueWithWeightedRisk_XlRegisterer;
