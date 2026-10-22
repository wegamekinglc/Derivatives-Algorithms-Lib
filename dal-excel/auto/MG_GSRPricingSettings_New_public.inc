

extern "C" __declspec(dllexport) OPER_* xl_GSRPricingSettings_New
    (const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("GSRPricingSettings_New");        
        argName = "settings (input #1)";
        const Matrix_<Cell_> settings = Excel::ToCellMatrix(xl_settings);
        const Excel::ScriptSettingsInput_ normalized_settings(xl_settings); xl_settings = normalized_settings.Get(); argName = "settings"; Excel::ValidateScriptSettingsRange(xl_settings, "GSRPricingSettings_New", "settings");
        argName = 0;
		Handle_<StorableGSRPricingSettings_> value;
        GSRPricingSettings_New(settings, &value);
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

struct XlRegister_GSRPricingSettings_New_
{
    XlRegister_GSRPricingSettings_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("quadrature_order and estimate_error field/value rows");
        Excel::Register("Base", "xl_GSRPricingSettings_New", "GSRPRICINGSETTINGS.NEW", "Wrap native Gaussian quadrature settings", "QQ", "settings", argHelp, false);
    }
};
static XlRegister_GSRPricingSettings_New_ The_GSRPricingSettings_New_XlRegisterer;
