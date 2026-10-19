

extern "C" __declspec(dllexport) OPER_* xl_MarketIvMarket_New
    (const OPER_* xl_name, const OPER_* xl_evaluation_date, const OPER_* xl_spot, const OPER_* xl_terms)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("MarketIvMarket_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        Excel::ValidateMarketIvNumber(xl_evaluation_date, "evaluation_date"); const Excel::ScriptSettingsInput_ normalized_evaluation_date(xl_evaluation_date); xl_evaluation_date = normalized_evaluation_date.Get(); Excel::ValidateMarketIvNumber(xl_spot, "spot"); const Excel::ScriptSettingsInput_ normalized_spot(xl_spot); xl_spot = normalized_spot.Get(); Excel::ValidateMarketIvRange(xl_terms, "terms"); const Excel::ScriptSettingsInput_ normalized_terms(xl_terms); xl_terms = normalized_terms.Get();
        argName = "evaluation_date (input #2)";
        const double evaluation_date = Excel::ToDouble(xl_evaluation_date);
        argName = "spot (input #3)";
        const double spot = Excel::ToDouble(xl_spot);
        argName = "terms (input #4)";
        const Matrix_<Cell_> terms = Excel::ToCellMatrix(xl_terms, false, true);
        argName = 0;
		Handle_<StorableMarketIvMarket_> market;
        MarketIvMarket_New(name, evaluation_date, spot, terms, &market);
        Excel::Retval_ retval;
        retval.Load(_env, market);
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

struct XlRegister_MarketIvMarket_New_
{
    XlRegister_MarketIvMarket_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("name");
        argHelp.push_back("evaluation date");
        argHelp.push_back("spot");
        argHelp.push_back("terms");
        Excel::Register("Base", "xl_MarketIvMarket_New", "MARKETIVMARKET.NEW", "Market IV surface owning value", "QQQQQ", "name,evaluation_date,spot,terms", argHelp, false);
    }
};
static XlRegister_MarketIvMarket_New_ The_MarketIvMarket_New_XlRegisterer;
