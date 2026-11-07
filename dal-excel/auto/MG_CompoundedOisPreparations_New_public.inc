

extern "C" __declspec(dllexport) OPER_* xl_CompoundedOisPreparations_New
    (const OPER_* xl_tables)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CompoundedOisPreparations_New");        
        argName = "tables (input #1)";
        const Vector_<Handle_<Storable_>> tables = Excel::ToHandleBaseVector(_env, xl_tables);
        argName = 0;
		Handle_<StorableCompoundedOisPreparations_> value;
        CompoundedOisPreparations_New(tables, &value);
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

struct XlRegister_CompoundedOisPreparations_New_
{
    XlRegister_CompoundedOisPreparations_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Prepared table handles");
        Excel::Register("Base", "xl_CompoundedOisPreparations_New", "COMPOUNDEDOISPREPARATIONS.NEW", "Same core compounded OIS interface", "QQ", "tables", argHelp, false);
    }
};
static XlRegister_CompoundedOisPreparations_New_ The_CompoundedOisPreparations_New_XlRegisterer;
