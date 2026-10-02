

extern "C" __declspec(dllexport) OPER_* xl_GSRLeverageData_New
    (const OPER_* xl_name, const OPER_* xl_rateShifts, const OPER_* xl_times, const OPER_* xl_values)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("GSRLeverageData_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "rateShifts (input #2)";
        const Vector_<double> rateShifts = Excel::ToDoubleVector(xl_rateShifts);
        argName = "times (input #3)";
        const Vector_<double> times = Excel::ToDoubleVector(xl_times);
        argName = "values (input #4)";
        const Matrix_<double> values = Excel::ToDoubleMatrix(xl_values);
        argName = 0;
		Handle_<GSRLeverageData_> leverage;
        GSRLeverageData_New(name, rateShifts, times, values, &leverage);
        Excel::Retval_ retval;
        retval.Load(_env, leverage);
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

struct XlRegister_GSRLeverageData_New_
{
    XlRegister_GSRLeverageData_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Leverage data name");
        argHelp.push_back("Increasing short-rate shifts, including negative values");
        argHelp.push_back("Increasing nonnegative ACT/365 times");
        argHelp.push_back("Positive leverage, rate-shift rows and time columns");
        Excel::Register("Base", "xl_GSRLeverageData_New", "GSRLEVERAGEDATA.NEW", "Positive rate-shift leverage surface", "QQQQQ", "name,rateShifts,times,values", argHelp, false);
    }
};
static XlRegister_GSRLeverageData_New_ The_GSRLeverageData_New_XlRegisterer;
