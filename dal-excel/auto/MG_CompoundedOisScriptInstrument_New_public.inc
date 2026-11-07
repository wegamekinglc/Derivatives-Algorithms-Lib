

extern "C" __declspec(dllexport) OPER_* xl_CompoundedOisScriptInstrument_New
    (const OPER_* xl_tables)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CompoundedOisScriptInstrument_New");        
        argName = "tables (input #1)";
        const Handle_<StorablePreparedCompoundedOis_> tables = Excel::ToHandle<StorablePreparedCompoundedOis_>(_env, xl_tables);
        argName = 0;
		Handle_<CalibrationInstrument_> instrument;
        CompoundedOisScriptInstrument_New(tables, &instrument);
        Excel::Retval_ retval;
        retval.Load(_env, instrument);
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

struct XlRegister_CompoundedOisScriptInstrument_New_
{
    XlRegister_CompoundedOisScriptInstrument_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Frozen validated tables");
        Excel::Register("Base", "xl_CompoundedOisScriptInstrument_New", "COMPOUNDEDOISSCRIPTINSTRUMENT.NEW", "Same core compounded OIS interface", "QQ", "tables", argHelp, false);
    }
};
static XlRegister_CompoundedOisScriptInstrument_New_ The_CompoundedOisScriptInstrument_New_XlRegisterer;
