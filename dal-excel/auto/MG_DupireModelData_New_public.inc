

extern "C" __declspec(dllexport) OPER_* xl_DupireModelData_New
    (const OPER_* xl_name, const OPER_* xl_calibration, const OPER_* xl_index, const OPER_* xl_currency, const OPER_* xl_factor, const OPER_* xl_maxStep)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("DupireModelData_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "name"; Excel::ValidateDupireTextInput(xl_name, "DupireModelData_New; name");
        argName = "calibration (input #2)";
        const Handle_<StorableDupireCalibration_> calibration = Excel::ToHandle<StorableDupireCalibration_>(_env, xl_calibration);
        argName = "index (input #3)";
        const String_ index = Excel::ToString(xl_index);
        argName = "index"; Excel::ValidateDupireTextInput(xl_index, "DupireModelData_New; index");
        argName = "currency (input #4)";
        const String_ currency = Excel::ToString(xl_currency);
        argName = "currency"; Excel::ValidateDupireTextInput(xl_currency, "DupireModelData_New; currency");
        argName = "factor (input #5)";
        const String_ factor = Excel::ToString(xl_factor);
        argName = "factor"; Excel::ValidateDupireTextInput(xl_factor, "DupireModelData_New; factor"); const Excel::ScriptSettingsInput_ maxStepInput(xl_maxStep); xl_maxStep = maxStepInput.Get();
        argName = "maxStep (input #6)";
        const double maxStep = Excel::ToDouble(xl_maxStep, true).value_or(0.08333333333333333);
        argName = 0;
		Handle_<ModelData_> model;
        DupireModelData_New(name, calibration, index, currency, factor, maxStep, &model);
        Excel::Retval_ retval;
        retval.Load(_env, model);
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

struct XlRegister_DupireModelData_New_
{
    XlRegister_DupireModelData_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Object name");
        argHelp.push_back("Frozen calibration");
        argHelp.push_back("Equity index, for example EQ[LOCAL]");
        argHelp.push_back("Domestic currency");
        argHelp.push_back("Single Brownian factor name");
        argHelp.push_back("Positive model time step");
        Excel::Register("Base", "xl_DupireModelData_New", "DUPIREMODELDATA.NEW", "Create a Hybrid local-vol model from frozen calibration", "QQQQQQQ", "name,calibration,index,currency,factor,[maxStep]", argHelp, false);
    }
};
static XlRegister_DupireModelData_New_ The_DupireModelData_New_XlRegisterer;
