

extern "C" __declspec(dllexport) OPER_* xl_HybridConstantCorrelationData_New
    (const OPER_* xl_name, const OPER_* xl_factors, const OPER_* xl_correlations)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("HybridConstantCorrelationData_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "factors (input #2)";
        const Vector_<String_> factors = Excel::ToStringVector(xl_factors);
        argName = "correlations (input #3)";
        const Matrix_<double> correlations = Excel::ToDoubleMatrix(xl_correlations);
        argName = 0;
		Handle_<HybridCorrelationData_> provider;
        HybridConstantCorrelationData_New(name, factors, correlations, &provider);
        Excel::Retval_ retval;
        retval.Load(_env, provider);
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

struct XlRegister_HybridConstantCorrelationData_New_
{
    XlRegister_HybridConstantCorrelationData_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Provider name");
        argHelp.push_back("Factor names in matrix order");
        argHelp.push_back("Positive-definite correlation matrix");
        Excel::Register("Base", "xl_HybridConstantCorrelationData_New", "HYBRIDCONSTANTCORRELATIONDATA.NEW", "Create a named constant-factor correlation provider", "QQQQ", "name,factors,correlations", argHelp, false);
    }
};
static XlRegister_HybridConstantCorrelationData_New_ The_HybridConstantCorrelationData_New_XlRegisterer;
