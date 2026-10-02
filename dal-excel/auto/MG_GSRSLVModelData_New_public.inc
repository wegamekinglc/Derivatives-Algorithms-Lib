

extern "C" __declspec(dllexport) OPER_* xl_GSRSLVModelData_New
    (const OPER_* xl_name, const OPER_* xl_gaussian, const OPER_* xl_leverage, const OPER_* xl_varianceCorrelations, const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("GSRSLVModelData_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "gaussian (input #2)";
        const Handle_<ModelData_> gaussian = Excel::ToHandle<ModelData_>(_env, xl_gaussian);
        argName = "leverage (input #3)";
        const Handle_<GSRLeverageData_> leverage = Excel::ToHandle<GSRLeverageData_>(_env, xl_leverage);
        argName = "varianceCorrelations (input #4)";
        const Vector_<double> varianceCorrelations = Excel::ToDoubleVector(xl_varianceCorrelations, true);
        argName = "settings (input #5)";
        const Matrix_<Cell_> settings = Excel::ToCellMatrix(xl_settings, true);
        argName = 0;
		Handle_<ModelData_> model;
        GSRSLVModelData_New(name, gaussian, leverage, varianceCorrelations, settings, &model);
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

struct XlRegister_GSRSLVModelData_New_
{
    XlRegister_GSRSLVModelData_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Model name");
        argHelp.push_back("MultiFactorGSRModelData with initial curves and g/H inputs");
        argHelp.push_back("Positive rate-shift leverage");
        argHelp.push_back("One rate/variance correlation per factor; default zero");
        argHelp.push_back("Key/value rows: kappa (1), volOfVol (0.5), maxStep (1/52 years)");
        Excel::Register("Base", "xl_GSRSLVModelData_New", "GSRSLVMODELDATA.NEW", "Compose GSR stochastic local volatility with normalized CIR variance", "QQQQQQ", "name,gaussian,leverage,[varianceCorrelations],[settings]", argHelp, false);
    }
};
static XlRegister_GSRSLVModelData_New_ The_GSRSLVModelData_New_XlRegisterer;
