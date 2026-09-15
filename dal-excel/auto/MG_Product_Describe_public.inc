

extern "C" __declspec(dllexport) OPER_* xl_Product_Describe
    (const OPER_* xl_product)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("Product_Describe");        
        argName = "product (input #1)";
        const Handle_<ScriptProductData_> product = Excel::ToHandle<ScriptProductData_>(_env, xl_product);
        argName = 0;
		Vector_<String_> json;
        Product_Describe(product, &json);
        Excel::Retval_ retval;
        retval.Load(json);
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

struct XlRegister_Product_Describe_
{
    XlRegister_Product_Describe_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Script product handle");
        Excel::Register("Base", "xl_Product_Describe", "PRODUCT.DESCRIBE", "Describe contract syntax without date capture, history reads or model setup", "QQ", "product", argHelp, false);
    }
};
static XlRegister_Product_Describe_ The_Product_Describe_XlRegisterer;
