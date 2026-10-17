

extern "C" __declspec(dllexport) OPER_* xl_RiskResult_Get_Shape
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("RiskResult_Get_Shape");        
        argName = "result (input #1)";
        const Handle_<StorableRiskResult_> result = Excel::ToHandle<StorableRiskResult_>(_env, xl_result);
        argName = 0;
		Matrix_<Cell_> shape;
        RiskResult_Get_Shape(result, &shape);
        Excel::Retval_ retval;
        retval.Load(shape);
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

struct XlRegister_RiskResult_Get_Shape_
{
    XlRegister_RiskResult_Get_Shape_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Completed result");
        Excel::Register("Base", "xl_RiskResult_Get_Shape", "RISKRESULT.GET.SHAPE", "Extract the exact numeric Jacobian dimensions", "QQ", "result", argHelp, false);
    }
};
static XlRegister_RiskResult_Get_Shape_ The_RiskResult_Get_Shape_XlRegisterer;
