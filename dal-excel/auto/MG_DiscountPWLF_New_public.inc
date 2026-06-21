

extern "C" __declspec(dllexport) OPER_* xl_DiscountPWLF_New
    (const OPER_* xl_name, const OPER_* xl_ccy, const OPER_* xl_knotDates, const OPER_* xl_fwdRates, const OPER_* xl_base)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("DiscountPWLF_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "ccy (input #2)";
        const String_ ccy = Excel::ToString(xl_ccy);
        argName = "knotDates (input #3)";
        const Vector_<Date_> knotDates = Excel::ToDateVector(xl_knotDates);
        argName = "fwdRates (input #4)";
        const Vector_<double> fwdRates = Excel::ToDoubleVector(xl_fwdRates);
      REQUIRE(fwdRates.size() == knotDates.size(), "must have one rate per knot date");
        argName = "base (input #5)";
        const Handle_<StorableDiscountCurve_> base = Excel::ToHandle<StorableDiscountCurve_>(_env, xl_base, true);
        argName = 0;
		Handle_<StorableDiscountCurve_> curve;
        DiscountPWLF_New(name, ccy, knotDates, fwdRates, base, &curve);
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

struct XlRegister_DiscountPWLF_New_
{
    XlRegister_DiscountPWLF_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("A name for the curve");
        argHelp.push_back("Currency code (e.g. \"USD\")");
        argHelp.push_back("Knot dates for the curve");
        argHelp.push_back("Forward rates at each knot date");
        argHelp.push_back("Optional base discount curve");
        Excel::Register("Base", "xl_DiscountPWLF_New", "DA.DISCOUNTPWLFNEW", "Build a discount curve from knot dates and forward rates (piecewise-linear forward)", "QQQQQQ", "name,ccy,knotDates,fwdRates,[base]", argHelp, false);
    }
};
static XlRegister_DiscountPWLF_New_ The_DiscountPWLF_New_XlRegisterer;
