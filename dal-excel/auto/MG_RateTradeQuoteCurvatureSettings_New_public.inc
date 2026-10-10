

extern "C" __declspec(dllexport) OPER_* xl_RateTradeQuoteCurvatureSettings_New
    (const OPER_* xl_name, const OPER_* xl_weights, const OPER_* xl_fixings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("RateTradeQuoteCurvatureSettings_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "name"; Excel::ValidateRiskRequestText(xl_name, "RateTradeQuoteCurvatureSettings_New; name"); argName = "weights"; Excel::ValidateCurvatureNumbers(xl_weights, "RateTradeQuoteCurvatureSettings_New", "weights"); const Excel::ScriptSettingsInput_ normalizedNumbers(xl_weights); xl_weights = normalizedNumbers.Get();
        argName = "weights (input #2)";
        const Matrix_<Cell_> weights = Excel::ToCellMatrix(xl_weights, true, true);
        argName = "fixings (input #3)";
        const Handle_<StorableMarketFixingSnapshot_> fixings = Excel::ToHandle<StorableMarketFixingSnapshot_>(_env, xl_fixings, true);
        argName = 0;
		Handle_<StorableRateTradeQuoteCurvatureSettings_> settings;
        RateTradeQuoteCurvatureSettings_New(name, weights, fixings, &settings);
        Excel::Retval_ retval;
        retval.Load(_env, settings);
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

struct XlRegister_RateTradeQuoteCurvatureSettings_New_
{
    XlRegister_RateTradeQuoteCurvatureSettings_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Object name");
        argHelp.push_back("Finite row or column weights; blank means unit weights; signed and zero weights allowed");
        argHelp.push_back("Explicit historical observations; blank permits native capture of missing trade history");
        Excel::Register("Base", "xl_RateTradeQuoteCurvatureSettings_New", "RATETRADEQUOTECURVATURESETTINGS.NEW", "Copy native rate portfolio weights and optional fixing observations", "QQQQ", "name,[weights],[fixings]", argHelp, false);
    }
};
static XlRegister_RateTradeQuoteCurvatureSettings_New_ The_RateTradeQuoteCurvatureSettings_New_XlRegisterer;
