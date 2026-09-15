

extern "C" __declspec(dllexport) OPER_* xl_ScriptValuation_Explain
    (const OPER_* xl_product, const OPER_* xl_modelData, const OPER_* xl_valuation)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("ScriptValuation_Explain");        
        argName = "product (input #1)";
        const Handle_<ScriptProductData_> product = Excel::ToHandle<ScriptProductData_>(_env, xl_product);
        argName = "modelData (input #2)";
        const Handle_<ModelData_> modelData = Excel::ToHandle<ModelData_>(_env, xl_modelData);
        xl_valuation = Excel::ScriptScalarInput(xl_valuation);
        argName = "valuation (input #3)";
        const Handle_<StorableScriptValuationSettings_> valuation = Excel::ToHandle<StorableScriptValuationSettings_>(_env, xl_valuation, true);
        argName = 0;
		Vector_<String_> json;
        ScriptValuation_Explain(product, modelData, valuation, &json);
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

struct XlRegister_ScriptValuation_Explain_
{
    XlRegister_ScriptValuation_Explain_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Script product handle");
        argHelp.push_back("BS or Dupire model data handle");
        argHelp.push_back("Valuation settings handle; blank selects per-call defaults");
        Excel::Register("Base", "xl_ScriptValuation_Explain", "SCRIPTVALUATION.EXPLAIN", "Prepare once with default price settings; no paths, workers or cache", "QQQQ", "product,modelData,[valuation]", argHelp, false);
    }
};
static XlRegister_ScriptValuation_Explain_ The_ScriptValuation_Explain_XlRegisterer;
