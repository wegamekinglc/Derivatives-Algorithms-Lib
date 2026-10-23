

extern "C" __declspec(dllexport) OPER_* xl_XccyGaussianDynamicsSpec_New
    (const OPER_* xl_settings, const OPER_* xl_buckets, const OPER_* xl_correlation)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("XccyGaussianDynamicsSpec_New");        
        argName = "settings (input #1)";
        const Matrix_<Cell_> settings = Excel::ToCellMatrix(xl_settings);
        const Excel::ScriptSettingsInput_ normalized_settings(xl_settings); xl_settings = normalized_settings.Get(); argName = "settings"; Excel::ValidateScriptSettingsRange(xl_settings, "XccyGaussianDynamicsSpec_New", "settings");
        argName = "buckets (input #2)";
        const Matrix_<Cell_> buckets = Excel::ToCellMatrix(xl_buckets);
        const Excel::ScriptSettingsInput_ normalized_buckets(xl_buckets); xl_buckets = normalized_buckets.Get(); argName = "buckets"; Excel::ValidateXccyTable(xl_buckets, "XccyGaussianDynamicsSpec_New", "buckets", 4);
        argName = "correlation (input #3)";
        const Matrix_<Cell_> correlation = Excel::ToCellMatrix(xl_correlation);
        const Excel::ScriptSettingsInput_ normalized_correlation(xl_correlation); xl_correlation = normalized_correlation.Get(); argName = "correlation"; Excel::ValidateXccyTable(xl_correlation, "XccyGaussianDynamicsSpec_New", "correlation", 4);
        argName = 0;
		Handle_<StorableXccyGaussianDynamicsSpec_> value;
        XccyGaussianDynamicsSpec_New(settings, buckets, correlation, &value);
        Excel::Retval_ retval;
        retval.Load(_env, value);
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

struct XlRegister_XccyGaussianDynamicsSpec_New_
{
    XlRegister_XccyGaussianDynamicsSpec_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Typed settings");
        argHelp.push_back("Typed buckets");
        argHelp.push_back("Typed correlation");
        Excel::Register("Base", "xl_XccyGaussianDynamicsSpec_New", "XCCYGAUSSIANDYNAMICSSPEC.NEW", "XccyGaussianDynamicsSpec New", "QQQQ", "settings,buckets,correlation", argHelp, false);
    }
};
static XlRegister_XccyGaussianDynamicsSpec_New_ The_XccyGaussianDynamicsSpec_New_XlRegisterer;
