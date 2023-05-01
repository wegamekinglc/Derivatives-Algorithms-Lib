

extern "C" __declspec(dllexport) OPER_* xl_Product_New
    (const OPER_* xl_name, const OPER_* xl_dates, const OPER_* xl_events)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("Product_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "dates (input #2)";
        const Matrix_<Cell_> dates = Excel::ToCellMatrix(xl_dates);
        argName = "events (input #3)";
        const Vector_<String_> events = Excel::ToStringVector(xl_events);
        argName = 0;
		Handle_<ScriptProductData_> product;
        Product_New(name, dates, events, &product);
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

struct XlRegister_Product_New_
{
    XlRegister_Product_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("A name for the object being created");
        argHelp.push_back("The event dates");
        argHelp.push_back("The event strings");
        Excel::Register("Base", "xl_Product_New", "PRODUCT.NEW", "Create a product from description", "QQQQ", "name,dates,events", argHelp, false);
    }
};
static XlRegister_Product_New_ The_Product_New_XlRegisterer;
