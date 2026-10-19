

extern "C" __declspec(dllexport) OPER_* xl_MarketIvCalibrationSnapshot_Get_Market
    (const OPER_* xl_source)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("MarketIvCalibrationSnapshot_Get_Market");        
        argName = "source (input #1)";
        const Handle_<StorableMarketIvCalibrationSnapshot_> source = Excel::ToHandle<StorableMarketIvCalibrationSnapshot_>(_env, xl_source);
        argName = 0;
		Handle_<StorableMarketIvMarket_> value;
        MarketIvCalibrationSnapshot_Get_Market(source, &value);
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

struct XlRegister_MarketIvCalibrationSnapshot_Get_Market_
{
    XlRegister_MarketIvCalibrationSnapshot_Get_Market_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("source");
        Excel::Register("Base", "xl_MarketIvCalibrationSnapshot_Get_Market", "MARKETIVCALIBRATIONSNAPSHOT.GET.MARKET", "Market IV surface copied diagnostics", "QQ", "source", argHelp, false);
    }
};
static XlRegister_MarketIvCalibrationSnapshot_Get_Market_ The_MarketIvCalibrationSnapshot_Get_Market_XlRegisterer;
