

extern "C" __declspec(dllexport) OPER_* xl_DupireCalibration_New
    (const OPER_* xl_name, const OPER_* xl_base, const OPER_* xl_inputs)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("DupireCalibration_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "name"; Excel::ValidateDupireTextInput(xl_name, "DupireCalibration_New; name");
        argName = "base (input #2)";
        const Handle_<Storable_> base = Excel::ToHandle<Storable_>(_env, xl_base);
        argName = "inputs (input #3)";
        const Handle_<StorableDupireRiskInputs_> inputs = Excel::ToHandle<StorableDupireRiskInputs_>(_env, xl_inputs);
        argName = 0;
		Handle_<StorableDupireCalibration_> calibration;
        DupireCalibration_New(name, base, inputs, &calibration);
        Excel::Retval_ retval;
        retval.Load(_env, calibration);
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

struct XlRegister_DupireCalibration_New_
{
    XlRegister_DupireCalibration_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Object name");
        argHelp.push_back("Existing BS model or MertonIVS handle");
        argHelp.push_back("Quote and grid inputs");
        Excel::Register("Base", "xl_DupireCalibration_New", "DUPIRECALIBRATION.NEW", "Freeze checked discrete Dupire calibration", "QQQQ", "name,base,inputs", argHelp, false);
    }
};
static XlRegister_DupireCalibration_New_ The_DupireCalibration_New_XlRegisterer;
