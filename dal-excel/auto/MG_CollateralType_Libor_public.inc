

extern "C" __declspec(dllexport) OPER_* xl_CollateralType_Libor
    (const OPER_* xl_tenor)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CollateralType_Libor");        
        argName = "tenor (input #1)";
        const String_ tenor = Excel::ToString(xl_tenor);
        argName = 0;
		Handle_<StorableCollateralType_> collateral;
        CollateralType_Libor(tenor, &collateral);
        Excel::Retval_ retval;
        retval.Load(_env, collateral);
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

struct XlRegister_CollateralType_Libor_
{
    XlRegister_CollateralType_Libor_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("The Libor tenor (e.g. \"3M\", \"6M\", \"12M\")");
        Excel::Register("Base", "xl_CollateralType_Libor", "COLLATERALTYPE.LIBOR", "Create a Libor (GC) collateral type handle", "QQ", "tenor", argHelp, false);
    }
};
static XlRegister_CollateralType_Libor_ The_CollateralType_Libor_XlRegisterer;
