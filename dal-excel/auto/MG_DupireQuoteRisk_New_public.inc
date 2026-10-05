

extern "C" __declspec(dllexport) OPER_* xl_DupireQuoteRisk_New
    (const OPER_* xl_name, const OPER_* xl_calibration, const OPER_* xl_parameters, const OPER_* xl_direct)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("DupireQuoteRisk_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "name"; Excel::ValidateDupireTextInput(xl_name, "DupireQuoteRisk_New; name");
        argName = "calibration (input #2)";
        const Handle_<StorableDupireCalibration_> calibration = Excel::ToHandle<StorableDupireCalibration_>(_env, xl_calibration);
        argName = "parameters (input #3)";
        const Handle_<StorableDupireParameterAdjoints_> parameters = Excel::ToHandle<StorableDupireParameterAdjoints_>(_env, xl_parameters);
        xl_direct = Excel::ScriptScalarInput(xl_direct);
        argName = "direct (input #4)";
        const Handle_<StorableDupireDirectQuoteAdjoints_> direct = Excel::ToHandle<StorableDupireDirectQuoteAdjoints_>(_env, xl_direct, true);
        argName = 0;
		Handle_<StorableDupireQuoteRisk_> result;
        DupireQuoteRisk_New(name, calibration, parameters, direct, &result);
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

struct XlRegister_DupireQuoteRisk_New_
{
    XlRegister_DupireQuoteRisk_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Object name");
        argHelp.push_back("Frozen calibration");
        argHelp.push_back("Raw surface-node adjoints");
        argHelp.push_back("Blank omits the direct quote contribution");
        Excel::Register("Base", "xl_DupireQuoteRisk_New", "DUPIREQUOTERISK.NEW", "Pull local-vol node adjoints back to spread quotes", "QQQQQ", "name,calibration,parameters,[direct]", argHelp, false);
    }
};
static XlRegister_DupireQuoteRisk_New_ The_DupireQuoteRisk_New_XlRegisterer;
