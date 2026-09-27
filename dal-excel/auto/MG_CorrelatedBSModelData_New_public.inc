

extern "C" __declspec(dllexport) OPER_* xl_CorrelatedBSModelData_New
    (const OPER_* xl_name, const OPER_* xl_indices, const OPER_* xl_spots, const OPER_* xl_vols, const OPER_* xl_divs, const OPER_* xl_rate, const OPER_* xl_correlations)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CorrelatedBSModelData_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "indices (input #2)";
        const Vector_<String_> indices = Excel::ToStringVector(xl_indices);
        argName = "spots (input #3)";
        const Vector_<double> spots = Excel::ToDoubleVector(xl_spots);
        argName = "vols (input #4)";
        const Vector_<double> vols = Excel::ToDoubleVector(xl_vols);
        argName = "divs (input #5)";
        const Vector_<double> divs = Excel::ToDoubleVector(xl_divs);
        argName = "rate (input #6)";
        const double rate = Excel::ToDouble(xl_rate);
        argName = "correlations (input #7)";
        const Matrix_<double> correlations = Excel::ToDoubleMatrix(xl_correlations);
        argName = 0;
		Handle_<ModelData_> model;
        CorrelatedBSModelData_New(name, indices, spots, vols, divs, rate, correlations, &model);
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

struct XlRegister_CorrelatedBSModelData_New_
{
    XlRegister_CorrelatedBSModelData_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Model name");
        argHelp.push_back("EQ index names in factor order");
        argHelp.push_back("Initial spots");
        argHelp.push_back("Volatilities");
        argHelp.push_back("Dividend yields");
        argHelp.push_back("Domestic deterministic rate");
        argHelp.push_back("Correlation matrix in index order");
        Excel::Register("Base", "xl_CorrelatedBSModelData_New", "CORRELATEDBSMODELDATA.NEW", "Create correlated multi-equity Black-Scholes model data", "QQQQQQQQ", "name,indices,spots,vols,divs,rate,correlations", argHelp, false);
    }
};
static XlRegister_CorrelatedBSModelData_New_ The_CorrelatedBSModelData_New_XlRegisterer;
