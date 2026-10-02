

extern "C" __declspec(dllexport) OPER_* xl_FixedCoupon_New
    (const OPER_* xl_payment, const OPER_* xl_accrual)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("FixedCoupon_New");        
        argName = "payment (input #1)";
        const Date_ payment = Excel::ToDate(xl_payment);
        argName = "accrual (input #2)";
        const double accrual = Excel::ToDouble(xl_accrual);
        argName = 0;
		Handle_<StorableGSRFixedCoupon_> coupon;
        FixedCoupon_New(payment, accrual, &coupon);
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

struct XlRegister_FixedCoupon_New_
{
    XlRegister_FixedCoupon_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Payment date");
        argHelp.push_back("Coupon accrual fraction");
        Excel::Register("Base", "xl_FixedCoupon_New", "FIXEDCOUPON.NEW", "Create one fixed swap coupon", "QQQ", "payment,accrual", argHelp, false);
    }
};
static XlRegister_FixedCoupon_New_ The_FixedCoupon_New_XlRegisterer;
