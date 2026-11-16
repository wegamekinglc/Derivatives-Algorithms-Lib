

extern "C" __declspec(dllexport) OPER_* xl_DiscountCurve_Get_Factor
    (const OPER_* xl_curve, const OPER_* xl_fromDate, const OPER_* xl_toDate)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("DiscountCurve_Get_Factor");        
        argName = "curve (input #1)";
        const Handle_<StorableDiscountCurve_> curve = Excel::ToHandle<StorableDiscountCurve_>(_env, xl_curve);
        argName = "fromDate (input #2)";
        const Date_ fromDate = Excel::ToDate(xl_fromDate);
        argName = "toDate (input #3)";
        const Date_ toDate = Excel::ToDate(xl_toDate);
        argName = 0;
		double factor;
        DiscountCurve_Get_Factor(curve, fromDate, toDate, &factor);
        Excel::Retval_ retval;
        retval.Load(factor);
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

struct XlRegister_DiscountCurve_Get_Factor_
{
    XlRegister_DiscountCurve_Get_Factor_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Discount or projection curve");
        argHelp.push_back("Start date");
        argHelp.push_back("End date");
        Excel::Register("Base", "xl_DiscountCurve_Get_Factor", "DISCOUNTCURVE.GET.FACTOR", "Evaluate the discount factor between two dates", "QQQQ", "curve,fromDate,toDate", argHelp, false);
    }
};
static XlRegister_DiscountCurve_Get_Factor_ The_DiscountCurve_Get_Factor_XlRegisterer;
