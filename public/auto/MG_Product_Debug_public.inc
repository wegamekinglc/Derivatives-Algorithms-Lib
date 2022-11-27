

extern "C" __declspec(dllexport) OPER_* xl_Product_Debug
    (const OPER_* xl_product)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("Product_Debug");        
        argName = "product (input #1)";
        const Handle_<ScriptProductData_> product = Excel::ToHandle<ScriptProductData_>(_env, xl_product);
        argName = 0;
		Vector_<String_> out;
        Product_Debug(product, &out);
        Excel::Retval_ retval;
        retval.Load(out);
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

struct XlRegister_Product_Debug_
{
    XlRegister_Product_Debug_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("The product");
        Excel::Register("Base", "xl_Product_Debug", "PRODUCT.DEBUG", "output the debug information of a product", "QQ", "product", argHelp, false);
    }
};
static XlRegister_Product_Debug_ The_Product_Debug_XlRegisterer;
