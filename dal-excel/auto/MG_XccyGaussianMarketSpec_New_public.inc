

extern "C" __declspec(dllexport) OPER_* xl_XccyGaussianMarketSpec_New
    (const OPER_* xl_settings, const OPER_* xl_domestic, const OPER_* xl_foreign, const OPER_* xl_basis)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("XccyGaussianMarketSpec_New");        
        argName = "settings (input #1)";
        const Matrix_<Cell_> settings = Excel::ToCellMatrix(xl_settings);
        const Excel::ScriptSettingsInput_ normalized_settings(xl_settings); xl_settings = normalized_settings.Get(); argName = "settings"; Excel::ValidateScriptSettingsRange(xl_settings, "XccyGaussianMarketSpec_New", "settings");
        argName = "domestic (input #2)";
        const Handle_<StorableCurveBlock_> domestic = Excel::ToHandle<StorableCurveBlock_>(_env, xl_domestic);
        argName = "foreign (input #3)";
        const Handle_<StorableCurveBlock_> foreign = Excel::ToHandle<StorableCurveBlock_>(_env, xl_foreign);
        argName = "basis (input #4)";
        const Handle_<StorableDiscountCurve_> basis = Excel::ToHandle<StorableDiscountCurve_>(_env, xl_basis);
        argName = 0;
		Handle_<StorableXccyGaussianMarketSpec_> value;
        XccyGaussianMarketSpec_New(settings, domestic, foreign, basis, &value);
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

struct XlRegister_XccyGaussianMarketSpec_New_
{
    XlRegister_XccyGaussianMarketSpec_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Typed settings");
        argHelp.push_back("Typed domestic");
        argHelp.push_back("Typed foreign");
        argHelp.push_back("Typed basis");
        Excel::Register("Base", "xl_XccyGaussianMarketSpec_New", "XCCYGAUSSIANMARKETSPEC.NEW", "XccyGaussianMarketSpec New", "QQQQQ", "settings,domestic,foreign,basis", argHelp, false);
    }
};
static XlRegister_XccyGaussianMarketSpec_New_ The_XccyGaussianMarketSpec_New_XlRegisterer;
