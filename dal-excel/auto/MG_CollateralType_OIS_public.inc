

extern "C" __declspec(dllexport) OPER_* xl_CollateralType_OIS
    ()	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CollateralType_OIS");        
        argName = 0;
		Handle_<StorableCollateralType_> collateral;
        CollateralType_OIS(&collateral);
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

struct XlRegister_CollateralType_OIS_
{
    XlRegister_CollateralType_OIS_()
    {
        Vector_<String_> argHelp;        
        Excel::Register("Base", "xl_CollateralType_OIS", "COLLATERALTYPE.OIS", "Create an OIS collateral type handle", "Q", "", argHelp, false);
    }
};
static XlRegister_CollateralType_OIS_ The_CollateralType_OIS_XlRegisterer;
