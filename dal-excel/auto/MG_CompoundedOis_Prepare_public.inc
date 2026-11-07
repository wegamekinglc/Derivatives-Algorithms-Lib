

extern "C" __declspec(dllexport) OPER_* xl_CompoundedOis_Prepare
    (const OPER_* xl_spec, const OPER_* xl_valuation)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CompoundedOis_Prepare");        
        argName = "spec (input #1)";
        const Handle_<StorableCompoundedOisScriptSpec_> spec = Excel::ToHandle<StorableCompoundedOisScriptSpec_>(_env, xl_spec);
        argName = "valuation (input #2)";
        const Handle_<StorableScriptValuationSettings_> valuation = Excel::ToHandle<StorableScriptValuationSettings_>(_env, xl_valuation);
        argName = 0;
		Handle_<StorablePreparedCompoundedOis_> value;
        CompoundedOis_Prepare(spec, valuation, &value);
        Excel::Retval_ retval;
        retval.Load(_env, value);
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

struct XlRegister_CompoundedOis_Prepare_
{
    XlRegister_CompoundedOis_Prepare_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Explicit contract");
        argHelp.push_back("Explicit date policy and fixing snapshot");
        Excel::Register("Base", "xl_CompoundedOis_Prepare", "COMPOUNDEDOIS.PREPARE", "Same core compounded OIS interface", "QQQ", "spec,valuation", argHelp, false);
    }
};
static XlRegister_CompoundedOis_Prepare_ The_CompoundedOis_Prepare_XlRegisterer;
