

extern "C" __declspec(dllexport) OPER_* xl_GSRCaplet_New
    (const OPER_* xl_expiry, const OPER_* xl_coupon, const OPER_* xl_strike, const OPER_* xl_type)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("GSRCaplet_New");        
        argName = "expiry (input #1)";
        const Date_ expiry = Excel::ToDate(xl_expiry);
        argName = "coupon (input #2)";
        const Handle_<StorableGSRFloatingCoupon_> coupon = Excel::ToHandle<StorableGSRFloatingCoupon_>(_env, xl_coupon);
        argName = "strike (input #3)";
        const double strike = Excel::ToDouble(xl_strike);
        argName = "type (input #4)";
        const String_ type = Excel::ToString(xl_type);
        argName = 0;
		Handle_<StorableGSREuropeanOption_> option;
        GSRCaplet_New(expiry, coupon, strike, type, &option);
        Excel::Retval_ retval;
        retval.Load(_env, option);
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

struct XlRegister_GSRCaplet_New_
{
    XlRegister_GSRCaplet_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Index fixing date, on or before accrual start");
        argHelp.push_back("Underlying coupon");
        argHelp.push_back("Rate strike");
        argHelp.push_back("CALL for caplet or PUT for floorlet");
        Excel::Register("Base", "xl_GSRCaplet_New", "GSRCAPLET.NEW", "Create a caplet or floorlet", "QQQQQ", "expiry,coupon,strike,type", argHelp, false);
    }
};
static XlRegister_GSRCaplet_New_ The_GSRCaplet_New_XlRegisterer;
