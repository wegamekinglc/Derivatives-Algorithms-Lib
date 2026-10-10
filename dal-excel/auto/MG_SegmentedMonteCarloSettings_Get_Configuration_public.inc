

extern "C" __declspec(dllexport) OPER_* xl_SegmentedMonteCarloSettings_Get_Configuration
    (const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("SegmentedMonteCarloSettings_Get_Configuration");        
        argName = "settings (input #1)";
        const Handle_<StorableSegmentedMonteCarloSettings_> settings = Excel::ToHandle<StorableSegmentedMonteCarloSettings_>(_env, xl_settings);
        argName = "settings"; Excel::ValidateRiskRequestText(xl_settings, "SegmentedMonteCarloSettings_Get_Configuration; settings"); xl_settings = Excel::ScriptScalarInput(xl_settings);
        argName = 0;
		Matrix_<Cell_> configuration;
        SegmentedMonteCarloSettings_Get_Configuration(settings, &configuration);
        Excel::Retval_ retval;
        retval.Load(configuration);
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

struct XlRegister_SegmentedMonteCarloSettings_Get_Configuration_
{
    XlRegister_SegmentedMonteCarloSettings_Get_Configuration_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Copied segmented settings; blank uses native defaults");
        Excel::Register("Base", "xl_SegmentedMonteCarloSettings_Get_Configuration", "SEGMENTEDMONTECARLOSETTINGS.GET.CONFIGURATION", "Copy all eight requested segmented MC settings", "QQ", "settings", argHelp, false);
    }
};
static XlRegister_SegmentedMonteCarloSettings_Get_Configuration_ The_SegmentedMonteCarloSettings_Get_Configuration_XlRegisterer;
