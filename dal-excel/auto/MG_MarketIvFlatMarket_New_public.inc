

extern "C" __declspec(dllexport) OPER_* xl_MarketIvFlatMarket_New
    (const OPER_* xl_name, const OPER_* xl_evaluation_date, const OPER_* xl_spot, const OPER_* xl_rate, const OPER_* xl_dividend_yield, const OPER_* xl_expiries)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("MarketIvFlatMarket_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        Excel::ValidateMarketIvNumber(xl_evaluation_date, "evaluation_date"); const Excel::ScriptSettingsInput_ normalized_evaluation_date(xl_evaluation_date); xl_evaluation_date = normalized_evaluation_date.Get(); Excel::ValidateMarketIvNumber(xl_spot, "spot"); const Excel::ScriptSettingsInput_ normalized_spot(xl_spot); xl_spot = normalized_spot.Get(); Excel::ValidateMarketIvNumber(xl_rate, "rate"); const Excel::ScriptSettingsInput_ normalized_rate(xl_rate); xl_rate = normalized_rate.Get(); Excel::ValidateMarketIvNumber(xl_dividend_yield, "dividend_yield"); const Excel::ScriptSettingsInput_ normalized_dividend_yield(xl_dividend_yield); xl_dividend_yield = normalized_dividend_yield.Get(); Excel::ValidateMarketIvRange(xl_expiries, "expiries"); const Excel::ScriptSettingsInput_ normalized_expiries(xl_expiries); xl_expiries = normalized_expiries.Get();
        argName = "evaluation_date (input #2)";
        const double evaluation_date = Excel::ToDouble(xl_evaluation_date);
        argName = "spot (input #3)";
        const double spot = Excel::ToDouble(xl_spot);
        argName = "rate (input #4)";
        const double rate = Excel::ToDouble(xl_rate);
        argName = "dividend_yield (input #5)";
        const double dividend_yield = Excel::ToDouble(xl_dividend_yield);
        argName = "expiries (input #6)";
        const Matrix_<Cell_> expiries = Excel::ToCellMatrix(xl_expiries, false, true);
        argName = 0;
		Handle_<StorableMarketIvMarket_> market;
        MarketIvFlatMarket_New(name, evaluation_date, spot, rate, dividend_yield, expiries, &market);
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

struct XlRegister_MarketIvFlatMarket_New_
{
    XlRegister_MarketIvFlatMarket_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("name");
        argHelp.push_back("evaluation date");
        argHelp.push_back("spot");
        argHelp.push_back("rate");
        argHelp.push_back("dividend yield");
        argHelp.push_back("expiries");
        Excel::Register("Base", "xl_MarketIvFlatMarket_New", "MARKETIVFLATMARKET.NEW", "Market IV surface owning value", "QQQQQQQ", "name,evaluation_date,spot,rate,dividend_yield,expiries", argHelp, false);
    }
};
static XlRegister_MarketIvFlatMarket_New_ The_MarketIvFlatMarket_New_XlRegisterer;
