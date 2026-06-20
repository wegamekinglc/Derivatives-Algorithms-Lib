

extern "C" __declspec(dllexport) OPER_* xl_RateIndexConventionNew
    (const OPER_* xl_forecastTenor, const OPER_* xl_basis, const OPER_* xl_collateral, const OPER_* xl_useProjectionCurve)
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {
        Logging::Write("RateIndexConventionNew");
        argName = "forecastTenor (input #1)";
        const String_ forecastTenor = Excel::ToString(xl_forecastTenor);
        argName = "basis (input #2)";
        const String_ basis = Excel::ToString(xl_basis);
        argName = "collateral (input #3)";
        const String_ collateral = Excel::ToString(xl_collateral);
        argName = "useProjectionCurve (input #4)";
        const bool useProjectionCurve = Excel::ToBool(xl_useProjectionCurve, true).value_or(false);
        argName = 0;
		Handle_<StorableRateIndexConvention_> convention;
        RateIndexConventionNew(forecastTenor, basis, collateral, useProjectionCurve, &convention);
        Excel::Retval_ retval;
        retval.Load(_env, convention);
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

struct XlRegister_RateIndexConventionNew_
{
    XlRegister_RateIndexConventionNew_()
    {
        Vector_<String_> argHelp;
        argHelp.push_back("Forecast tenor as an ISO period (e.g. \"3M\", \"6M\")");
        argHelp.push_back("Day basis name (e.g. \"ACT_365F\", \"ACT_360\")");
        argHelp.push_back("Collateral type string (e.g. \"OIS\", \"GC\")");
        argHelp.push_back("Whether to use a projection curve (default false)");
        Excel::Register("Base", "xl_RateIndexConventionNew", "DA.RATE.INDEX.CONVENTION.NEW", "Create a rate index convention handle", "QQQQQ!", "forecastTenor,basis,collateral,useProjectionCurve", argHelp, false);
    }
};
static XlRegister_RateIndexConventionNew_ The_RateIndexConventionNew_XlRegisterer;
