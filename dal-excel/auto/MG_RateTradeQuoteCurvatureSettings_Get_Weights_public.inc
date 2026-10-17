

extern "C" __declspec(dllexport) OPER_* xl_RateTradeQuoteCurvatureSettings_Get_Weights
    (const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("RateTradeQuoteCurvatureSettings_Get_Weights");        
        argName = "settings (input #1)";
        const Handle_<StorableRateTradeQuoteCurvatureSettings_> settings = Excel::ToHandle<StorableRateTradeQuoteCurvatureSettings_>(_env, xl_settings);
        argName = 0;
		Matrix_<Cell_> weights;
        RateTradeQuoteCurvatureSettings_Get_Weights(settings, &weights);
        Excel::Retval_ retval;
        retval.Load(weights);
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

struct XlRegister_RateTradeQuoteCurvatureSettings_Get_Weights_
{
    XlRegister_RateTradeQuoteCurvatureSettings_Get_Weights_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Owning portfolio settings");
        Excel::Register("Base", "xl_RateTradeQuoteCurvatureSettings_Get_Weights", "RATETRADEQUOTECURVATURESETTINGS.GET.WEIGHTS", "Copy signed native rate portfolio weights", "QQ", "settings", argHelp, false);
    }
};
static XlRegister_RateTradeQuoteCurvatureSettings_Get_Weights_ The_RateTradeQuoteCurvatureSettings_Get_Weights_XlRegisterer;
