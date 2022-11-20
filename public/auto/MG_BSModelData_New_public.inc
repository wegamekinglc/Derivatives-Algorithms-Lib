

extern "C" __declspec(dllexport) OPER_* xl_BSModelData_New
    (const OPER_* xl_name, const OPER_* xl_spot, const OPER_* xl_vol, const OPER_* xl_rate, const OPER_* xl_div)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("BSModelData_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "spot (input #2)";
        const double spot = Excel::ToDouble(xl_spot);
        argName = "vol (input #3)";
        const double vol = Excel::ToDouble(xl_vol);
        argName = "rate (input #4)";
        const double rate = Excel::ToDouble(xl_rate);
        argName = "div (input #5)";
        const double div = Excel::ToDouble(xl_div);
        argName = 0;
		Handle_<ModelData_> model;
        BSModelData_New(name, spot, vol, rate, div, &model);
        Excel::Retval_ retval;
        retval.Load(_env, model);
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

struct XlRegister_BSModelData_New_
{
    XlRegister_BSModelData_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("A name for the object being created");
        argHelp.push_back("current spot value");
        argHelp.push_back("volatility of the underlying");
        argHelp.push_back("risk-free rate");
        argHelp.push_back("dividend rate");
        Excel::Register("Base", "xl_BSModelData_New", "BSMODELDATA.NEW", "Black - Scholes model's data description", "QQQQQQ", "name,spot,vol,rate,div", argHelp, false);
    }
};
static XlRegister_BSModelData_New_ The_BSModelData_New_XlRegisterer;
