

extern "C" __declspec(dllexport) OPER_* xl_PasteWithArgs
    (const OPER_* xl_func_name)	
{
    Excel::InitializeSessionIfNeeded();
	ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("PasteWithArgs");        
        argName = "func_name (input #1)";
        const String_ func_name = Excel::ToString(xl_func_name);
        argName = 0;
		String_ func_with_args;
        PasteWithArgs(func_name, &func_with_args);
        Excel::Retval_ retval;
        retval.Load(func_with_args);
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

struct XlRegister_PasteWithArgs_
{
    XlRegister_PasteWithArgs_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("The function for which to find arguments");
        Excel::Register("Bones", "xl_PasteWithArgs", "DA.PASTEWITHARGS", "Shows all the argument names of a function", "QQ", "func_name", argHelp, false);
    }
};
static XlRegister_PasteWithArgs_ The_PasteWithArgs_XlRegisterer;
