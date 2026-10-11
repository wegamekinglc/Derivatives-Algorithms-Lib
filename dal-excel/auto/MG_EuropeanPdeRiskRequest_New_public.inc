

extern "C" __declspec(dllexport) OPER_* xl_EuropeanPdeRiskRequest_New
    (const OPER_* xl_name, const OPER_* xl_rate, const OPER_* xl_volatility, const OPER_* xl_strike, const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("EuropeanPdeRiskRequest_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "name"; Excel::ValidateRiskRequestText(xl_name, "EuropeanPdeRiskRequest_New; name"); argName = "rate"; Excel::ValidateEuropeanPdeNumber(xl_rate, "rate"); xl_rate = Excel::ScriptScalarInput(xl_rate); const Excel::ScriptSettingsInput_ rateInput(xl_rate); xl_rate = rateInput.Get(); argName = "volatility"; Excel::ValidateEuropeanPdeNumber(xl_volatility, "volatility"); xl_volatility = Excel::ScriptScalarInput(xl_volatility); const Excel::ScriptSettingsInput_ volatilityInput(xl_volatility); xl_volatility = volatilityInput.Get(); argName = "strike"; Excel::ValidateEuropeanPdeNumber(xl_strike, "strike"); xl_strike = Excel::ScriptScalarInput(xl_strike); const Excel::ScriptSettingsInput_ strikeInput(xl_strike); xl_strike = strikeInput.Get(); xl_settings = Excel::ScriptScalarInput(xl_settings);
        argName = "rate (input #2)";
        const double rate = Excel::ToDouble(xl_rate);
        argName = "volatility (input #3)";
        const double volatility = Excel::ToDouble(xl_volatility);
        argName = "strike (input #4)";
        const double strike = Excel::ToDouble(xl_strike);
        argName = "settings (input #5)";
        const Handle_<StorableEuropeanPdeRiskSettings_> settings = Excel::ToHandle<StorableEuropeanPdeRiskSettings_>(_env, xl_settings, true);
        argName = 0;
		Handle_<StorableEuropeanPdeRiskRequest_> request;
        EuropeanPdeRiskRequest_New(name, rate, volatility, strike, settings, &request);
        Excel::Retval_ retval;
        retval.Load(_env, request);
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

struct XlRegister_EuropeanPdeRiskRequest_New_
{
    XlRegister_EuropeanPdeRiskRequest_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Object name");
        argHelp.push_back("Finite continuously compounded decimal rate");
        argHelp.push_back("Finite decimal volatility; positive domain checked at execution");
        argHelp.push_back("Finite strike price; domain and grid kink checked at execution");
        argHelp.push_back("Blank uses native defaults");
        Excel::Register("Base", "xl_EuropeanPdeRiskRequest_New", "EUROPEANPDERISKREQUEST.NEW", "Create a passive fixed-grid European call and put risk request", "QQQQQQ", "name,rate,volatility,strike,[settings]", argHelp, false);
    }
};
static XlRegister_EuropeanPdeRiskRequest_New_ The_EuropeanPdeRiskRequest_New_XlRegisterer;
