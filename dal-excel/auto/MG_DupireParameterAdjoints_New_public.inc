

extern "C" __declspec(dllexport) OPER_* xl_DupireParameterAdjoints_New
    (const OPER_* xl_name, const OPER_* xl_calibration, const OPER_* xl_adjoints)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("DupireParameterAdjoints_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "name"; Excel::ValidateDupireTextInput(xl_name, "DupireParameterAdjoints_New; name");
        argName = "calibration (input #2)";
        const Handle_<StorableDupireCalibration_> calibration = Excel::ToHandle<StorableDupireCalibration_>(_env, xl_calibration);
        const Excel::ScriptSettingsInput_ adjointsInput(xl_adjoints); xl_adjoints = adjointsInput.Get();
        argName = "adjoints (input #3)";
        const Matrix_<double> adjoints = Excel::ToDoubleMatrix(xl_adjoints);
        argName = 0;
		Handle_<StorableDupireParameterAdjoints_> parameters;
        DupireParameterAdjoints_New(name, calibration, adjoints, &parameters);
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

struct XlRegister_DupireParameterAdjoints_New_
{
    XlRegister_DupireParameterAdjoints_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Object name");
        argHelp.push_back("Frozen calibration");
        argHelp.push_back("Spot rows, model-time columns; finite raw PV adjoints");
        Excel::Register("Base", "xl_DupireParameterAdjoints_New", "DUPIREPARAMETERADJOINTS.NEW", "Create immutable local-vol node adjoints", "QQQQ", "name,calibration,adjoints", argHelp, false);
    }
};
static XlRegister_DupireParameterAdjoints_New_ The_DupireParameterAdjoints_New_XlRegisterer;
