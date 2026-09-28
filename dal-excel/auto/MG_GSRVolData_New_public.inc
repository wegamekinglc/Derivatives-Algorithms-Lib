

extern "C" __declspec(dllexport) OPER_* xl_GSRVolData_New
    (const OPER_* xl_name, const OPER_* xl_gKnotDates, const OPER_* xl_gValues, const OPER_* xl_hKnotDates, const OPER_* xl_hValues)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("GSRVolData_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "gKnotDates (input #2)";
        const Vector_<Date_> gKnotDates = Excel::ToDateVector(xl_gKnotDates);
        argName = "gValues (input #3)";
        const Vector_<double> gValues = Excel::ToDoubleVector(xl_gValues);
        argName = "hKnotDates (input #4)";
        const Vector_<Date_> hKnotDates = Excel::ToDateVector(xl_hKnotDates);
        argName = "hValues (input #5)";
        const Vector_<double> hValues = Excel::ToDoubleVector(xl_hValues);
        argName = 0;
		Handle_<GSRVolData_> vol;
        GSRVolData_New(name, gKnotDates, gValues, hKnotDates, hValues, &vol);
        Excel::Retval_ retval;
        retval.Load(_env, vol);
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

struct XlRegister_GSRVolData_New_
{
    XlRegister_GSRVolData_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Volatility data name");
        argHelp.push_back("State volatility knot dates");
        argHelp.push_back("Nonnegative state volatility values");
        argHelp.push_back("Bond loading knot dates");
        argHelp.push_back("Positive bond loading values");
        Excel::Register("Base", "xl_GSRVolData_New", "GSRVOLDATA.NEW", "Piecewise-constant g and H inputs for a GSR model", "QQQQQQ", "name,gKnotDates,gValues,hKnotDates,hValues", argHelp, false);
    }
};
static XlRegister_GSRVolData_New_ The_GSRVolData_New_XlRegisterer;
