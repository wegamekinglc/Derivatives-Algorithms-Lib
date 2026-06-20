

extern "C" __declspec(dllexport) OPER_* xl_CollateralTypeLibor
    (const OPER_* xl_tenor)
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {
        Logging::Write("CollateralTypeLibor");
        argName = "tenor (input #1)";
        const String_ tenor = Excel::ToString(xl_tenor);
        argName = 0;
		Handle_<StorableCollateralType_> collateral;
        CollateralTypeLibor(tenor, &collateral);
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

struct XlRegister_CollateralTypeLibor_
{
    XlRegister_CollateralTypeLibor_()
    {
        Vector_<String_> argHelp;
        argHelp.push_back("The Libor tenor (e.g. \"3M\", \"6M\", \"12M\")");
        Excel::Register("Base", "xl_CollateralTypeLibor", "DA.COLLATERAL.LIBOR", "Create a Libor (GC) collateral type handle", "QQ!", "tenor", argHelp, false);
    }
};
static XlRegister_CollateralTypeLibor_ The_CollateralTypeLibor_XlRegisterer;
