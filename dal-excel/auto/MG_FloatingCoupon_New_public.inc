

extern "C" __declspec(dllexport) OPER_* xl_FloatingCoupon_New
    (const OPER_* xl_fixing, const OPER_* xl_start, const OPER_* xl_end, const OPER_* xl_payment, const OPER_* xl_indexAccrual, const OPER_* xl_couponAccrual, const OPER_* xl_tenor)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("FloatingCoupon_New");        
        argName = "fixing (input #1)";
        const Date_ fixing = Excel::ToDate(xl_fixing);
        argName = "start (input #2)";
        const Date_ start = Excel::ToDate(xl_start);
        argName = "end (input #3)";
        const Date_ end = Excel::ToDate(xl_end);
        argName = "payment (input #4)";
        const Date_ payment = Excel::ToDate(xl_payment);
        argName = "indexAccrual (input #5)";
        const double indexAccrual = Excel::ToDouble(xl_indexAccrual);
        argName = "couponAccrual (input #6)";
        const double couponAccrual = Excel::ToDouble(xl_couponAccrual);
        argName = "tenor (input #7)";
        const String_ tenor = Excel::ToString(xl_tenor);
        argName = 0;
		Handle_<StorableFloatingCoupon_> coupon;
        FloatingCoupon_New(fixing, start, end, payment, indexAccrual, couponAccrual, tenor, &coupon);
        Excel::Retval_ retval;
        retval.Load(_env, coupon);
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

struct XlRegister_FloatingCoupon_New_
{
    XlRegister_FloatingCoupon_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Index fixing date, on or before start");
        argHelp.push_back("Accrual start date");
        argHelp.push_back("Accrual end date");
        argHelp.push_back("Payment date, on or after end");
        argHelp.push_back("Index accrual fraction");
        argHelp.push_back("Payment accrual fraction");
        argHelp.push_back("Projection tenor such as 3M");
        Excel::Register("Base", "xl_FloatingCoupon_New", "FLOATINGCOUPON.NEW", "Create one projected floating coupon", "QQQQQQQQ", "fixing,start,end,payment,indexAccrual,couponAccrual,tenor", argHelp, false);
    }
};
static XlRegister_FloatingCoupon_New_ The_FloatingCoupon_New_XlRegisterer;
