

extern "C" __declspec(dllexport) OPER_* xl_Product_Get_RiskOutputs
    (const OPER_* xl_product)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("Product_Get_RiskOutputs");        
        argName = "product (input #1)";
        const Handle_<ScriptProductData_> product = Excel::ToHandle<ScriptProductData_>(_env, xl_product);
        xl_product = Excel::ScriptScalarInput(xl_product);
        argName = 0;
		Matrix_<Cell_> outputs;
        Product_Get_RiskOutputs(product, &outputs);
        Excel::Retval_ retval;
        retval.Load(outputs);
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

struct XlRegister_Product_Get_RiskOutputs_
{
    XlRegister_Product_Get_RiskOutputs_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Product handle");
        Excel::Register("Base", "xl_Product_Get_RiskOutputs", "PRODUCT.GET.RISKOUTPUTS", "List indexed scalar output coordinates without valuation", "QQ", "product", argHelp, false);
    }
};
static XlRegister_Product_Get_RiskOutputs_ The_Product_Get_RiskOutputs_XlRegisterer;
