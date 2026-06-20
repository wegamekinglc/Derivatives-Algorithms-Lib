

extern "C" __declspec(dllexport) OPER_* xl_CurrencyPairNew
    (const OPER_* xl_domestic, const OPER_* xl_foreign)
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {
        Logging::Write("CurrencyPairNew");
        argName = "domestic (input #1)";
        const String_ domestic = Excel::ToString(xl_domestic);
        argName = "foreign (input #2)";
        const String_ foreign = Excel::ToString(xl_foreign);
        argName = 0;
		Handle_<StorableCurrencyPair_> pair;
        CurrencyPairNew(domestic, foreign, &pair);
        Excel::Retval_ retval;
        retval.Load(_env, pair);
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

struct XlRegister_CurrencyPairNew_
{
    XlRegister_CurrencyPairNew_()
    {
        Vector_<String_> argHelp;
        argHelp.push_back("Domestic currency code (e.g. \"USD\")");
        argHelp.push_back("Foreign currency code (e.g. \"EUR\")");
        Excel::Register("Base", "xl_CurrencyPairNew", "DA.CURRENCY.PAIR.NEW", "Create a currency pair handle", "QQQ!", "domestic,foreign", argHelp, false);
    }
};
static XlRegister_CurrencyPairNew_ The_CurrencyPairNew_XlRegisterer;
