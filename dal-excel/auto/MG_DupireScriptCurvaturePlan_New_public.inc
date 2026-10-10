

extern "C" __declspec(dllexport) OPER_* xl_DupireScriptCurvaturePlan_New
    (const OPER_* xl_name, const OPER_* xl_product, const OPER_* xl_modelData, const OPER_* xl_calibration, const OPER_* xl_component, const OPER_* xl_request)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("DupireScriptCurvaturePlan_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "name"; Excel::ValidateRiskRequestText(xl_name, "DupireScriptCurvaturePlan_New; name"); argName = "component"; Excel::ValidateRiskRequestText(xl_component, "DupireScriptCurvaturePlan_New; component");
        argName = "product (input #2)";
        const Handle_<ScriptProductData_> product = Excel::ToHandle<ScriptProductData_>(_env, xl_product);
        argName = "modelData (input #3)";
        const Handle_<ModelData_> modelData = Excel::ToHandle<ModelData_>(_env, xl_modelData);
        argName = "calibration (input #4)";
        const Handle_<StorableDupireCalibration_> calibration = Excel::ToHandle<StorableDupireCalibration_>(_env, xl_calibration);
        argName = "component (input #5)";
        const String_ component = Excel::ToString(xl_component);
        argName = "request (input #6)";
        const Handle_<StorableDupireScriptCurvatureRequest_> request = Excel::ToHandle<StorableDupireScriptCurvatureRequest_>(_env, xl_request);
        argName = 0;
		Handle_<StorableDupireScriptCurvaturePlan_> plan;
        DupireScriptCurvaturePlan_New(name, product, modelData, calibration, component, request, &plan);
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

struct XlRegister_DupireScriptCurvaturePlan_New_
{
    XlRegister_DupireScriptCurvaturePlan_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Object name");
        argHelp.push_back("European script product with rebuildable scalar quote bindings");
        argHelp.push_back("Matching native Hybrid model");
        argHelp.push_back("Frozen Dupire calibration and full strike-major quote order");
        argHelp.push_back("Native local-vol component name");
        argHelp.push_back("Copied first-order settings and full raw quote direction request");
        Excel::Register("Base", "xl_DupireScriptCurvaturePlan_New", "DUPIRESCRIPTCURVATUREPLAN.NEW", "Admit and freeze a common-path recalibrated Dupire curvature plan", "QQQQQQQ", "name,product,modelData,calibration,component,request", argHelp, false);
    }
};
static XlRegister_DupireScriptCurvaturePlan_New_ The_DupireScriptCurvaturePlan_New_XlRegisterer;
