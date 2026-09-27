

extern "C" __declspec(dllexport) OPER_* xl_HybridLogDfRateData_New
    (const OPER_* xl_name, const OPER_* xl_currency, const OPER_* xl_times, const OPER_* xl_logDF, const OPER_* xl_scheme)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("HybridLogDfRateData_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "currency (input #2)";
        const String_ currency = Excel::ToString(xl_currency);
        argName = "times (input #3)";
        const Vector_<double> times = Excel::ToDoubleVector(xl_times);
        argName = "logDF (input #4)";
        const Vector_<double> logDF = Excel::ToDoubleVector(xl_logDF);
        argName = "scheme (input #5)";
        const String_ scheme = Excel::ToString(xl_scheme, true);
        argName = 0;
		Handle_<HybridComponentData_> component;
        HybridLogDfRateData_New(name, currency, times, logDF, scheme, &component);
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

struct XlRegister_HybridLogDfRateData_New_
{
    XlRegister_HybridLogDfRateData_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Component name");
        argHelp.push_back("Domestic currency");
        argHelp.push_back("ACT/365 times from the valuation date, starting at zero");
        argHelp.push_back("Log discount factors, starting at zero");
        argHelp.push_back("Log discount-factor interpolation (default LOG_LINEAR)");
        Excel::Register("Base", "xl_HybridLogDfRateData_New", "HYBRIDLOGDFRATEDATA.NEW", "Create a domestic rate component from model-time log discount factors", "QQQQQQ", "name,currency,times,logDF,[scheme]", argHelp, false);
    }
};
static XlRegister_HybridLogDfRateData_New_ The_HybridLogDfRateData_New_XlRegisterer;
