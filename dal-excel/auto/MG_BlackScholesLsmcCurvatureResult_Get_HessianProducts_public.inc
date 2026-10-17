

extern "C" __declspec(dllexport) OPER_* xl_BlackScholesLsmcCurvatureResult_Get_HessianProducts
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("BlackScholesLsmcCurvatureResult_Get_HessianProducts");        
        argName = "result (input #1)";
        const Handle_<StorableBlackScholesLsmcCurvatureResult_> result = Excel::ToHandle<StorableBlackScholesLsmcCurvatureResult_>(_env, xl_result);
        argName = "result"; Excel::ValidateRiskRequestText(xl_result, "BlackScholesLsmcCurvatureResult_Get_HessianProducts; result"); xl_result = Excel::ScriptScalarInput(xl_result);
        argName = 0;
		Matrix_<Cell_> hessianproducts;
        BlackScholesLsmcCurvatureResult_Get_HessianProducts(result, &hessianproducts);
        Excel::Retval_ retval;
        retval.Load(hessianproducts);
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

struct XlRegister_BlackScholesLsmcCurvatureResult_Get_HessianProducts_
{
    XlRegister_BlackScholesLsmcCurvatureResult_Get_HessianProducts_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Stored immutable result");
        Excel::Register("Base", "xl_BlackScholesLsmcCurvatureResult_Get_HessianProducts", "BLACKSCHOLESLSMCCURVATURERESULT.GET.HESSIANPRODUCTS", "Copy BlackScholesLsmcCurvatureResult Get HessianProducts", "QQ", "result", argHelp, false);
    }
};
static XlRegister_BlackScholesLsmcCurvatureResult_Get_HessianProducts_ The_BlackScholesLsmcCurvatureResult_Get_HessianProducts_XlRegisterer;
