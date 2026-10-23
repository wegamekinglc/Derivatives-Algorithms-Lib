

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
        const Matrix_<Cell_> settings = Excel::ToCellMatrix_orXccy(xl_settings, Excel::XccyTableInput_("XccyGaussianDynamicsSpec_New", "settings", 2), false, true);
        argName = "buckets (input #2)";
        const Matrix_<Cell_> buckets = Excel::ToCellMatrix_orXccy(xl_buckets, Excel::XccyTableInput_("XccyGaussianDynamicsSpec_New", "buckets", 4), false, true);
        argName = "correlation (input #3)";
        const Matrix_<Cell_> correlation = Excel::ToCellMatrix_orXccy(xl_correlation, Excel::XccyTableInput_("XccyGaussianDynamicsSpec_New", "correlation", 4), false, true);
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
