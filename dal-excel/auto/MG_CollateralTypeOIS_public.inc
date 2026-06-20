

extern "C" __declspec(dllexport) OPER_* xl_CollateralTypeOIS
    ()
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {
        Logging::Write("CollateralTypeOIS");
        argName = 0;
		Handle_<StorableCollateralType_> collateral;
        CollateralTypeOIS(&collateral);
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

struct XlRegister_CollateralTypeOIS_
{
    XlRegister_CollateralTypeOIS_()
    {
        Vector_<String_> argHelp;
        Excel::Register("Base", "xl_CollateralTypeOIS", "DA.COLLATERAL.OIS", "Create an OIS collateral type handle", "Q!", "", argHelp, false);
    }
};
static XlRegister_CollateralTypeOIS_ The_CollateralTypeOIS_XlRegisterer;
