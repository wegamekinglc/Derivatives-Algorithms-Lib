

extern "C" __declspec(dllexport) OPER_* xl_DupireDirectQuoteAdjoints_New
    (const OPER_* xl_name, const OPER_* xl_calibration, const OPER_* xl_adjoints)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("DupireDirectQuoteAdjoints_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "name"; Excel::ValidateDupireTextInput(xl_name, "DupireDirectQuoteAdjoints_New; name");
        argName = "calibration (input #2)";
        const Handle_<StorableDupireCalibration_> calibration = Excel::ToHandle<StorableDupireCalibration_>(_env, xl_calibration);
        const Excel::ScriptSettingsInput_ adjointsInput(xl_adjoints); xl_adjoints = adjointsInput.Get();
        argName = "adjoints (input #3)";
        const Matrix_<double> adjoints = Excel::ToDoubleMatrix(xl_adjoints);
        argName = 0;
		Handle_<StorableDupireDirectQuoteAdjoints_> direct;
        DupireDirectQuoteAdjoints_New(name, calibration, adjoints, &direct);
        Excel::Retval_ retval;
        retval.Load(_env, direct);
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

struct XlRegister_DupireDirectQuoteAdjoints_New_
{
    XlRegister_DupireDirectQuoteAdjoints_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Object name");
        argHelp.push_back("Frozen calibration");
        argHelp.push_back("Strike rows, maturity columns; finite direct PV adjoints");
        Excel::Register("Base", "xl_DupireDirectQuoteAdjoints_New", "DUPIREDIRECTQUOTEADJOINTS.NEW", "Create immutable direct quote adjoints", "QQQQ", "name,calibration,adjoints", argHelp, false);
    }
};
static XlRegister_DupireDirectQuoteAdjoints_New_ The_DupireDirectQuoteAdjoints_New_XlRegisterer;
