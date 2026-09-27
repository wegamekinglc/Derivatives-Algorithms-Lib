

extern "C" __declspec(dllexport) OPER_* xl_HybridDeterministicRateData_New
    (const OPER_* xl_name, const OPER_* xl_currency, const OPER_* xl_rate)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("HybridDeterministicRateData_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "currency (input #2)";
        const String_ currency = Excel::ToString(xl_currency);
        argName = "rate (input #3)";
        const double rate = Excel::ToDouble(xl_rate);
        argName = 0;
		Handle_<HybridComponentData_> component;
        HybridDeterministicRateData_New(name, currency, rate, &component);
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

struct XlRegister_HybridDeterministicRateData_New_
{
    XlRegister_HybridDeterministicRateData_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Component name");
        argHelp.push_back("Domestic currency");
        argHelp.push_back("Continuous rate");
        Excel::Register("Base", "xl_HybridDeterministicRateData_New", "HYBRIDDETERMINISTICRATEDATA.NEW", "Create a deterministic domestic-rate component", "QQQQ", "name,currency,rate", argHelp, false);
    }
};
static XlRegister_HybridDeterministicRateData_New_ The_HybridDeterministicRateData_New_XlRegisterer;
