

extern "C" __declspec(dllexport) OPER_* xl_DupireScriptQuoteRisk_New
    (const OPER_* xl_name, const OPER_* xl_valuation, const OPER_* xl_calibration, const OPER_* xl_component, const OPER_* xl_direct)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("DupireScriptQuoteRisk_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "name"; Excel::ValidateDupireTextInput(xl_name, "DupireScriptQuoteRisk_New; name");
        argName = "valuation (input #2)";
        const Handle_<StorableRiskResult_> valuation = Excel::ToHandle<StorableRiskResult_>(_env, xl_valuation);
        argName = "calibration (input #3)";
        const Handle_<StorableDupireCalibration_> calibration = Excel::ToHandle<StorableDupireCalibration_>(_env, xl_calibration);
        argName = "component (input #4)";
        const String_ component = Excel::ToString(xl_component);
        argName = "component"; Excel::ValidateDupireTextInput(xl_component, "DupireScriptQuoteRisk_New; component"); xl_direct = Excel::ScriptScalarInput(xl_direct);
        argName = "direct (input #5)";
        const Handle_<StorableDupireDirectQuoteAdjoints_> direct = Excel::ToHandle<StorableDupireDirectQuoteAdjoints_>(_env, xl_direct, true);
        argName = 0;
		Handle_<StorableDupireScriptQuoteRisk_> result;
        DupireScriptQuoteRisk_New(name, valuation, calibration, component, direct, &result);
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

struct XlRegister_DupireScriptQuoteRisk_New_
{
    XlRegister_DupireScriptQuoteRisk_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Object name");
        argHelp.push_back("Completed scalar valuation risk");
        argHelp.push_back("Frozen calibration");
        argHelp.push_back("Local-vol component name");
        argHelp.push_back("Blank omits the direct quote contribution");
        Excel::Register("Base", "xl_DupireScriptQuoteRisk_New", "DUPIRESCRIPTQUOTERISK.NEW", "Map completed valuation risk to frozen spread quotes", "QQQQQQ", "name,valuation,calibration,component,[direct]", argHelp, false);
    }
};
static XlRegister_DupireScriptQuoteRisk_New_ The_DupireScriptQuoteRisk_New_XlRegisterer;
