

extern "C" __declspec(dllexport) OPER_* xl_CalibrationParameterAdjoints_Get_Adjoints
    (const OPER_* xl_parameters)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CalibrationParameterAdjoints_Get_Adjoints");        
        argName = "parameters (input #1)";
        const Handle_<StorableCalibrationParameterAdjoints_> parameters = Excel::ToHandle<StorableCalibrationParameterAdjoints_>(_env, xl_parameters);
        argName = 0;
		Matrix_<double> adjoints;
        CalibrationParameterAdjoints_Get_Adjoints(parameters, &adjoints);
        Excel::Retval_ retval;
        retval.Load(adjoints);
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

struct XlRegister_CalibrationParameterAdjoints_Get_Adjoints_
{
    XlRegister_CalibrationParameterAdjoints_Get_Adjoints_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Owned parameter seeds");
        Excel::Register("Base", "xl_CalibrationParameterAdjoints_Get_Adjoints", "CALIBRATIONPARAMETERADJOINTS.GET.ADJOINTS", "Copy canonical parameter seeds", "QQ", "parameters", argHelp, false);
    }
};
static XlRegister_CalibrationParameterAdjoints_Get_Adjoints_ The_CalibrationParameterAdjoints_Get_Adjoints_XlRegisterer;
