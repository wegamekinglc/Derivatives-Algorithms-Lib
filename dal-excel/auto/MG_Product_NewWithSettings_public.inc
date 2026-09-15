

extern "C" __declspec(dllexport) OPER_* xl_Product_NewWithSettings
    (const OPER_* xl_name, const OPER_* xl_dates, const OPER_* xl_events, const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("Product_NewWithSettings");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "dates (input #2)";
        const Vector_<Cell_> dates = Excel::ToCellVector(xl_dates);
        argName = "events (input #3)";
        const Vector_<String_> events = Excel::ToStringVector(xl_events);
        argName = "settings (input #4)";
        const Handle_<StorableScriptProductSettings_> settings = Excel::ToHandle<StorableScriptProductSettings_>(_env, xl_settings);
        argName = 0;
		Handle_<ScriptProductData_> product;
        Product_NewWithSettings(name, dates, events, settings, &product);
        Excel::Retval_ retval;
        retval.Load(_env, product);
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

struct XlRegister_Product_NewWithSettings_
{
    XlRegister_Product_NewWithSettings_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Object name");
        argHelp.push_back("Event dates or definition labels; same conversion as PRODUCT.NEW");
        argHelp.push_back("Event script text; use FIX(EQ[AAPL]) without index quotes");
        argHelp.push_back("Required SCRIPTPRODUCTSETTINGS.NEW handle");
        Excel::Register("Base", "xl_Product_NewWithSettings", "PRODUCT.NEWWITHSETTINGS", "Create a script product using contract settings", "QQQQQ", "name,dates,events,settings", argHelp, false);
    }
};
static XlRegister_Product_NewWithSettings_ The_Product_NewWithSettings_XlRegisterer;
