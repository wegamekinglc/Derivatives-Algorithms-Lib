

extern "C" __declspec(dllexport) OPER_* xl_JacobianRiskResult_Get_Product
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("JacobianRiskResult_Get_Product");        
        argName = "result (input #1)";
        const Handle_<StorableJacobianRiskResult_> result = Excel::ToHandle<StorableJacobianRiskResult_>(_env, xl_result);
        argName = 0;
		Matrix_<Cell_> product;
        JacobianRiskResult_Get_Product(result, &product);
        Excel::Retval_ retval;
        retval.Load(product);
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

struct XlRegister_JacobianRiskResult_Get_Product_
{
    XlRegister_JacobianRiskResult_Get_Product_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Completed result");
        Excel::Register("Base", "xl_JacobianRiskResult_Get_Product", "JACOBIANRISKRESULT.GET.PRODUCT", "Extract the frozen source product table", "QQ", "result", argHelp, false);
    }
};
static XlRegister_JacobianRiskResult_Get_Product_ The_JacobianRiskResult_Get_Product_XlRegisterer;
