

extern "C" __declspec(dllexport) OPER_* xl_XccyMonteCarloValidation_Get_Flags
    (const OPER_* xl_value)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("XccyMonteCarloValidation_Get_Flags");        
        argName = "value (input #1)";
        const Handle_<StorableXccyMonteCarloValidation_> value = Excel::ToHandle<StorableXccyMonteCarloValidation_>(_env, xl_value);
        argName = 0;
		Matrix_<Cell_> flags;
        XccyMonteCarloValidation_Get_Flags(value, &flags);
        Excel::Retval_ retval;
        retval.Load(flags);
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

struct XlRegister_XccyMonteCarloValidation_Get_Flags_
{
    XlRegister_XccyMonteCarloValidation_Get_Flags_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Typed value");
        Excel::Register("Base", "xl_XccyMonteCarloValidation_Get_Flags", "XCCYMONTECARLOVALIDATION.GET.FLAGS", "XccyMonteCarloValidation Get Flags", "QQ", "value", argHelp, false);
    }
};
static XlRegister_XccyMonteCarloValidation_Get_Flags_ The_XccyMonteCarloValidation_Get_Flags_XlRegisterer;
