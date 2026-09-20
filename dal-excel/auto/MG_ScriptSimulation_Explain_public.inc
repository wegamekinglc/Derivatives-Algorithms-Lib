

extern "C" __declspec(dllexport) OPER_* xl_ScriptSimulation_Explain
    (const OPER_* xl_product, const OPER_* xl_modelData, const OPER_* xl_n_paths, const OPER_* xl_valuation, const OPER_* xl_simulation)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("ScriptSimulation_Explain");        
        argName = "product (input #1)";
        const Handle_<ScriptProductData_> product = Excel::ToHandle<ScriptProductData_>(_env, xl_product);
        argName = "modelData (input #2)";
        const Handle_<ModelData_> modelData = Excel::ToHandle<ModelData_>(_env, xl_modelData);
        argName = "n_paths (input #3)";
        const double n_paths = Excel::ToDouble(xl_n_paths);
        xl_valuation = Excel::ScriptScalarInput(xl_valuation); xl_simulation = Excel::ScriptScalarInput(xl_simulation);
        argName = "valuation (input #4)";
        const Handle_<StorableScriptValuationSettings_> valuation = Excel::ToHandle<StorableScriptValuationSettings_>(_env, xl_valuation, true);
        argName = "simulation (input #5)";
        const Handle_<StorableMonteCarloSettings_> simulation = Excel::ToHandle<StorableMonteCarloSettings_>(_env, xl_simulation, true);
        argName = 0;
		Vector_<String_> json;
        ScriptSimulation_Explain(product, modelData, n_paths, valuation, simulation, &json);
        Excel::Retval_ retval;
        retval.Load(json);
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

struct XlRegister_ScriptSimulation_Explain_
{
    XlRegister_ScriptSimulation_Explain_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Script product handle");
        argHelp.push_back("BS or Dupire model data handle");
        argHelp.push_back("Finite integer from 1 to INT_MAX");
        argHelp.push_back("Valuation settings handle; blank selects per-call defaults");
        argHelp.push_back("Monte Carlo settings handle; blank uses sobol, no BB/AAD, smooth 0.01 and tree");
        Excel::Register("Base", "xl_ScriptSimulation_Explain", "SCRIPTSIMULATION.EXPLAIN", "Run the full double valuation and report per-exercise-event LSMC diagnostics", "QQQQQQ", "product,modelData,n_paths,[valuation],[simulation]", argHelp, false);
    }
};
static XlRegister_ScriptSimulation_Explain_ The_ScriptSimulation_Explain_XlRegisterer;
