

extern "C" __declspec(dllexport) OPER_* xl_MultiFactorGSRVolData_New
    (const OPER_* xl_name, const OPER_* xl_factorNames, const OPER_* xl_gKnotDates, const OPER_* xl_gValues, const OPER_* xl_hKnotDates, const OPER_* xl_hValues, const OPER_* xl_correlations)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("MultiFactorGSRVolData_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "factorNames (input #2)";
        const Vector_<String_> factorNames = Excel::ToStringVector(xl_factorNames);
        argName = "gKnotDates (input #3)";
        const Vector_<Date_> gKnotDates = Excel::ToDateVector(xl_gKnotDates);
        argName = "gValues (input #4)";
        const Matrix_<double> gValues = Excel::ToDoubleMatrix(xl_gValues);
        argName = "hKnotDates (input #5)";
        const Vector_<Date_> hKnotDates = Excel::ToDateVector(xl_hKnotDates);
        argName = "hValues (input #6)";
        const Matrix_<double> hValues = Excel::ToDoubleMatrix(xl_hValues);
        argName = "correlations (input #7)";
        const Matrix_<double> correlations = Excel::ToDoubleMatrix(xl_correlations);
        argName = 0;
		Handle_<MultiFactorGSRVolData_> vol;
        MultiFactorGSRVolData_New(name, factorNames, gKnotDates, gValues, hKnotDates, hValues, correlations, &vol);
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

struct XlRegister_MultiFactorGSRVolData_New_
{
    XlRegister_MultiFactorGSRVolData_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Volatility data name");
        argHelp.push_back("Factor names in Brownian order");
        argHelp.push_back("State volatility knot dates");
        argHelp.push_back("Nonnegative volatility, factor rows and date columns");
        argHelp.push_back("Bond loading knot dates");
        argHelp.push_back("Signed loading, factor rows and date columns");
        argHelp.push_back("Positive semidefinite factor correlation matrix");
        Excel::Register("Base", "xl_MultiFactorGSRVolData_New", "MULTIFACTORGSRVOLDATA.NEW", "Named multi-factor Gaussian rate volatility", "QQQQQQQQ", "name,factorNames,gKnotDates,gValues,hKnotDates,hValues,correlations", argHelp, false);
    }
};
static XlRegister_MultiFactorGSRVolData_New_ The_MultiFactorGSRVolData_New_XlRegisterer;
