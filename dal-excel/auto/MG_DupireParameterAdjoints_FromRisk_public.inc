

extern "C" __declspec(dllexport) OPER_* xl_DupireParameterAdjoints_FromRisk
    (const OPER_* xl_name, const OPER_* xl_valuation, const OPER_* xl_calibration, const OPER_* xl_component)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("DupireParameterAdjoints_FromRisk");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "name"; Excel::ValidateDupireTextInput(xl_name, "DupireParameterAdjoints_FromRisk; name");
        argName = "valuation (input #2)";
        const Handle_<StorableRiskResult_> valuation = Excel::ToHandle<StorableRiskResult_>(_env, xl_valuation);
        argName = "calibration (input #3)";
        const Handle_<StorableDupireCalibration_> calibration = Excel::ToHandle<StorableDupireCalibration_>(_env, xl_calibration);
        argName = "component (input #4)";
        const String_ component = Excel::ToString(xl_component);
        argName = "component"; Excel::ValidateDupireTextInput(xl_component, "DupireParameterAdjoints_FromRisk; component");
        argName = 0;
		Handle_<StorableDupireParameterAdjoints_> parameters;
        DupireParameterAdjoints_FromRisk(name, valuation, calibration, component, &parameters);
        Excel::Retval_ retval;
        retval.Load(_env, parameters);
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

struct XlRegister_DupireParameterAdjoints_FromRisk_
{
    XlRegister_DupireParameterAdjoints_FromRisk_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Object name");
        argHelp.push_back("Completed scalar valuation risk");
        argHelp.push_back("Frozen calibration");
        argHelp.push_back("Local-vol component name, equity for DupireModelData_New");
        Excel::Register("Base", "xl_DupireParameterAdjoints_FromRisk", "DUPIREPARAMETERADJOINTS.FROMRISK", "Extract all local-vol node adjoints from a valuation", "QQQQQ", "name,valuation,calibration,component", argHelp, false);
    }
};
static XlRegister_DupireParameterAdjoints_FromRisk_ The_DupireParameterAdjoints_FromRisk_XlRegisterer;
