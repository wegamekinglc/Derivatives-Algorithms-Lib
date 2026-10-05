

extern "C" __declspec(dllexport) OPER_* xl_DupireScriptRiskPlan_New
    (const OPER_* xl_name, const OPER_* xl_product, const OPER_* xl_modelData, const OPER_* xl_calibration, const OPER_* xl_component, const OPER_* xl_request)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("DupireScriptRiskPlan_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "name"; Excel::ValidateRiskRequestText(xl_name, "DupireScriptRiskPlan_New; name");
        argName = "product (input #2)";
        const Handle_<ScriptProductData_> product = Excel::ToHandle<ScriptProductData_>(_env, xl_product);
        argName = "modelData (input #3)";
        const Handle_<ModelData_> modelData = Excel::ToHandle<ModelData_>(_env, xl_modelData);
        argName = "calibration (input #4)";
        const Handle_<StorableDupireCalibration_> calibration = Excel::ToHandle<StorableDupireCalibration_>(_env, xl_calibration);
        argName = "component"; Excel::ValidateRiskRequestText(xl_component, "DupireScriptRiskPlan_New; component"); xl_component = Excel::ScriptScalarInput(xl_component);
        argName = "component (input #5)";
        const String_ component = Excel::ToString(xl_component);
        argName = "request (input #6)";
        const Handle_<StorableDupireScriptRiskRequest_> request = Excel::ToHandle<StorableDupireScriptRiskRequest_>(_env, xl_request);
        argName = 0;
		Handle_<StorableDupireScriptRiskPlan_> plan;
        DupireScriptRiskPlan_New(name, product, modelData, calibration, component, request, &plan);
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

struct XlRegister_DupireScriptRiskPlan_New_
{
    XlRegister_DupireScriptRiskPlan_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Object name");
        argHelp.push_back("Scalar script payoff");
        argHelp.push_back("Native single-currency Hybrid with deterministic flat rate");
        argHelp.push_back("Frozen target Dupire calibration");
        argHelp.push_back("Exact target local-vol component name");
        argHelp.push_back("Required automatic quote and execution request");
        Excel::Register("Base", "xl_DupireScriptRiskPlan_New", "DUPIRESCRIPTRISKPLAN.NEW", "Seal an automatic Dupire valuation before history or worker activity", "QQQQQQQ", "name,product,modelData,calibration,component,request", argHelp, false);
    }
};
static XlRegister_DupireScriptRiskPlan_New_ The_DupireScriptRiskPlan_New_XlRegisterer;
