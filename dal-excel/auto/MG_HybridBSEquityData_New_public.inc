

extern "C" __declspec(dllexport) OPER_* xl_HybridBSEquityData_New
    (const OPER_* xl_name, const OPER_* xl_index, const OPER_* xl_currency, const OPER_* xl_factor, const OPER_* xl_spot, const OPER_* xl_vol, const OPER_* xl_div)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("HybridBSEquityData_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "index (input #2)";
        const String_ index = Excel::ToString(xl_index);
        argName = "currency (input #3)";
        const String_ currency = Excel::ToString(xl_currency);
        argName = "factor (input #4)";
        const String_ factor = Excel::ToString(xl_factor);
        argName = "spot (input #5)";
        const double spot = Excel::ToDouble(xl_spot);
        argName = "vol (input #6)";
        const double vol = Excel::ToDouble(xl_vol);
        argName = "div (input #7)";
        const double div = Excel::ToDouble(xl_div);
        argName = 0;
		Handle_<HybridComponentData_> component;
        HybridBSEquityData_New(name, index, currency, factor, spot, vol, div, &component);
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

struct XlRegister_HybridBSEquityData_New_
{
    XlRegister_HybridBSEquityData_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Component name");
        argHelp.push_back("EQ index name");
        argHelp.push_back("Domestic currency");
        argHelp.push_back("Named Brownian factor");
        argHelp.push_back("Initial spot");
        argHelp.push_back("Volatility");
        argHelp.push_back("Dividend yield");
        Excel::Register("Base", "xl_HybridBSEquityData_New", "HYBRIDBSEQUITYDATA.NEW", "Create a named hybrid Black-Scholes equity component", "QQQQQQQQ", "name,index,currency,factor,spot,vol,div", argHelp, false);
    }
};
static XlRegister_HybridBSEquityData_New_ The_HybridBSEquityData_New_XlRegisterer;
