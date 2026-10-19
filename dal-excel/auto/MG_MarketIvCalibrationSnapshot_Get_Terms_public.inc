

extern "C" __declspec(dllexport) OPER_* xl_MarketIvCalibrationSnapshot_Get_Terms
    (const OPER_* xl_source)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("MarketIvCalibrationSnapshot_Get_Terms");        
        argName = "source (input #1)";
        const Handle_<StorableMarketIvCalibrationSnapshot_> source = Excel::ToHandle<StorableMarketIvCalibrationSnapshot_>(_env, xl_source);
        argName = 0;
		Matrix_<Cell_> table;
        MarketIvCalibrationSnapshot_Get_Terms(source, &table);
        Excel::Retval_ retval;
        retval.Load(table);
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

struct XlRegister_MarketIvCalibrationSnapshot_Get_Terms_
{
    XlRegister_MarketIvCalibrationSnapshot_Get_Terms_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("source");
        Excel::Register("Base", "xl_MarketIvCalibrationSnapshot_Get_Terms", "MARKETIVCALIBRATIONSNAPSHOT.GET.TERMS", "Market IV surface copied diagnostics", "QQ", "source", argHelp, false);
    }
};
static XlRegister_MarketIvCalibrationSnapshot_Get_Terms_ The_MarketIvCalibrationSnapshot_Get_Terms_XlRegisterer;
