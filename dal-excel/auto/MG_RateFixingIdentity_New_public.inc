

extern "C" __declspec(dllexport) OPER_* xl_RateFixingIdentity_New
    (const OPER_* xl_indexName, const OPER_* xl_hour, const OPER_* xl_minute)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("RateFixingIdentity_New");        
        argName = "indexName (input #1)";
        const String_ indexName = Excel::ToString(xl_indexName);
        argName = "hour (input #2)";
        const int hour = Excel::ToInt(xl_hour);
        argName = "minute (input #3)";
        const int minute = Excel::ToInt(xl_minute);
        argName = 0;
		Handle_<StorableFixingIdentity_> identity;
        RateFixingIdentity_New(indexName, hour, minute, &identity);
        Excel::Retval_ retval;
        retval.Load(_env, identity);
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

struct XlRegister_RateFixingIdentity_New_
{
    XlRegister_RateFixingIdentity_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Fixing index name (e.g. \"USD-LIBOR-3M\")");
        argHelp.push_back("Fixing publication hour (0-23)");
        argHelp.push_back("Fixing publication minute (0-59)");
        Excel::Register("Base", "xl_RateFixingIdentity_New", "RATEFIXINGIDENTITY.NEW", "Create a rate fixing publication identity", "QQQQ", "indexName,hour,minute", argHelp, false);
    }
};
static XlRegister_RateFixingIdentity_New_ The_RateFixingIdentity_New_XlRegisterer;
