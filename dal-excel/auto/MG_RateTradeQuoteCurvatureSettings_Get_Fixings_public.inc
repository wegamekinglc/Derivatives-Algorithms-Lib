

extern "C" __declspec(dllexport) OPER_* xl_RateTradeQuoteCurvatureSettings_Get_Fixings
    (const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("RateTradeQuoteCurvatureSettings_Get_Fixings");        
        argName = "settings (input #1)";
        const Handle_<StorableRateTradeQuoteCurvatureSettings_> settings = Excel::ToHandle<StorableRateTradeQuoteCurvatureSettings_>(_env, xl_settings);
        argName = 0;
		Matrix_<Cell_> fixings;
        RateTradeQuoteCurvatureSettings_Get_Fixings(settings, &fixings);
        Excel::Retval_ retval;
        retval.Load(fixings);
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

struct XlRegister_RateTradeQuoteCurvatureSettings_Get_Fixings_
{
    XlRegister_RateTradeQuoteCurvatureSettings_Get_Fixings_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Owning portfolio settings");
        Excel::Register("Base", "xl_RateTradeQuoteCurvatureSettings_Get_Fixings", "RATETRADEQUOTECURVATURESETTINGS.GET.FIXINGS", "Copy saved fixing records and explicit snapshot presence", "QQ", "settings", argHelp, false);
    }
};
static XlRegister_RateTradeQuoteCurvatureSettings_Get_Fixings_ The_RateTradeQuoteCurvatureSettings_Get_Fixings_XlRegisterer;
