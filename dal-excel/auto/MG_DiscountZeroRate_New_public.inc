

extern "C" __declspec(dllexport) OPER_* xl_DiscountZeroRate_New
    (const OPER_* xl_name, const OPER_* xl_ccy, const OPER_* xl_anchorDate, const OPER_* xl_nodeDates, const OPER_* xl_zeroRates, const OPER_* xl_dayCount, const OPER_* xl_logDfScheme, const OPER_* xl_base)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("DiscountZeroRate_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "ccy (input #2)";
        const String_ ccy = Excel::ToString(xl_ccy);
        argName = "anchorDate (input #3)";
        const Date_ anchorDate = Excel::ToDate(xl_anchorDate);
        argName = "nodeDates (input #4)";
        const Vector_<Date_> nodeDates = Excel::ToDateVector(xl_nodeDates);
        argName = "zeroRates (input #5)";
        const Vector_<double> zeroRates = Excel::ToDoubleVector(xl_zeroRates);
      REQUIRE(zeroRates.size() == nodeDates.size(), "must have one zero rate per node date");
        argName = "dayCount (input #6)";
        const String_ dayCount = Excel::ToString(xl_dayCount, true);
        argName = "logDfScheme (input #7)";
        const String_ logDfScheme = Excel::ToString(xl_logDfScheme, true);
        argName = "base (input #8)";
        const Handle_<StorableDiscountCurve_> base = Excel::ToHandle<StorableDiscountCurve_>(_env, xl_base, true);
        argName = 0;
		Handle_<StorableDiscountCurve_> curve;
        DiscountZeroRate_New(name, ccy, anchorDate, nodeDates, zeroRates, dayCount, logDfScheme, base, &curve);
        Excel::Retval_ retval;
        retval.Load(_env, curve);
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

struct XlRegister_DiscountZeroRate_New_
{
    XlRegister_DiscountZeroRate_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("A name for the curve");
        argHelp.push_back("Currency code (e.g. \"USD\")");
        argHelp.push_back("Anchor date for the zero rates");
        argHelp.push_back("Future node dates for the curve");
        argHelp.push_back("Continuously compounded zero rates at each node date");
        argHelp.push_back("Day-count basis used to map zero rates to log discount factors (default \"ACT_365F\")");
        argHelp.push_back("Log discount-factor interpolation scheme (default \"LOG_LINEAR\")");
        argHelp.push_back("Optional base discount curve");
        Excel::Register("Base", "xl_DiscountZeroRate_New", "DISCOUNTZERORATE.NEW", "Build a discount curve from continuously compounded zero rates", "QQQQQQQQQ", "name,ccy,anchorDate,nodeDates,zeroRates,[dayCount],[logDfScheme],[base]", argHelp, false);
    }
};
static XlRegister_DiscountZeroRate_New_ The_DiscountZeroRate_New_XlRegisterer;
