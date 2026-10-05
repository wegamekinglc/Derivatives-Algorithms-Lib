

extern "C" __declspec(dllexport) OPER_* xl_DupireParameterAdjoints_Get_Adjoints
    (const OPER_* xl_parameters)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("DupireParameterAdjoints_Get_Adjoints");        
        argName = "parameters (input #1)";
        const Handle_<StorableDupireParameterAdjoints_> parameters = Excel::ToHandle<StorableDupireParameterAdjoints_>(_env, xl_parameters);
        argName = 0;
		Matrix_<double> adjoints;
        DupireParameterAdjoints_Get_Adjoints(parameters, &adjoints);
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

struct XlRegister_DupireParameterAdjoints_Get_Adjoints_
{
    XlRegister_DupireParameterAdjoints_Get_Adjoints_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Surface-node seeds");
        Excel::Register("Base", "xl_DupireParameterAdjoints_Get_Adjoints", "DUPIREPARAMETERADJOINTS.GET.ADJOINTS", "Copy local-vol node adjoints", "QQ", "parameters", argHelp, false);
    }
};
static XlRegister_DupireParameterAdjoints_Get_Adjoints_ The_DupireParameterAdjoints_Get_Adjoints_XlRegisterer;
