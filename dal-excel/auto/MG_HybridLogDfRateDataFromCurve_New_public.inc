

extern "C" __declspec(dllexport) OPER_* xl_HybridLogDfRateDataFromCurve_New
    (const OPER_* xl_name, const OPER_* xl_curve, const OPER_* xl_evaluationDate, const OPER_* xl_nodeDates, const OPER_* xl_scheme)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("HybridLogDfRateDataFromCurve_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "curve (input #2)";
        const Handle_<StorableDiscountCurve_> curve = Excel::ToHandle<StorableDiscountCurve_>(_env, xl_curve);
        argName = "evaluationDate (input #3)";
        const Date_ evaluationDate = Excel::ToDate(xl_evaluationDate);
        argName = "nodeDates (input #4)";
        const Vector_<Date_> nodeDates = Excel::ToDateVector(xl_nodeDates);
        argName = "scheme (input #5)";
        const String_ scheme = Excel::ToString(xl_scheme, true);
        argName = 0;
		Handle_<HybridComponentData_> component;
        HybridLogDfRateDataFromCurve_New(name, curve, evaluationDate, nodeDates, scheme, &component);
        Excel::Retval_ retval;
        retval.Load(_env, component);
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

struct XlRegister_HybridLogDfRateDataFromCurve_New_
{
    XlRegister_HybridLogDfRateDataFromCurve_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Component name");
        argHelp.push_back("Calibrated domestic discount curve");
        argHelp.push_back("Model valuation date");
        argHelp.push_back("Increasing snapshot dates starting at the valuation date");
        argHelp.push_back("Log discount-factor interpolation (default LOG_LINEAR)");
        Excel::Register("Base", "xl_HybridLogDfRateDataFromCurve_New", "HYBRIDLOGDFRATEDATAFROMCURVE.NEW", "Snapshot a dated discount curve on the hybrid model time axis", "QQQQQQ", "name,curve,evaluationDate,nodeDates,[scheme]", argHelp, false);
    }
};
static XlRegister_HybridLogDfRateDataFromCurve_New_ The_HybridLogDfRateDataFromCurve_New_XlRegisterer;
