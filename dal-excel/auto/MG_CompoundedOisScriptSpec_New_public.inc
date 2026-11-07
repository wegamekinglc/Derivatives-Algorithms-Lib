

extern "C" __declspec(dllexport) OPER_* xl_CompoundedOisScriptSpec_New
    (const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CompoundedOisScriptSpec_New");        
        argName = "settings (input #1)";
        const Matrix_<Cell_> settings = Excel::ToCellMatrix_orXccy(xl_settings, Excel::XccyTableInput_("CompoundedOisScriptSpec_New", "settings", 2), false, true);
        argName = 0;
		Handle_<StorableCompoundedOisScriptSpec_> value;
        CompoundedOisScriptSpec_New(settings, &value);
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

struct XlRegister_CompoundedOisScriptSpec_New_
{
    XlRegister_CompoundedOisScriptSpec_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Required named columns or exact field/value rows");
        Excel::Register("Base", "xl_CompoundedOisScriptSpec_New", "COMPOUNDEDOISSCRIPTSPEC.NEW", "Create immutable typed compounded OIS input", "QQ", "settings", argHelp, false);
    }
};
static XlRegister_CompoundedOisScriptSpec_New_ The_CompoundedOisScriptSpec_New_XlRegisterer;
