

extern "C" __declspec(dllexport) OPER_* xl_WeightedRiskResult_Get_Shape
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("WeightedRiskResult_Get_Shape");        
        argName = "result (input #1)";
        const Handle_<StorableWeightedRiskResult_> result = Excel::ToHandle<StorableWeightedRiskResult_>(_env, xl_result);
        argName = 0;
		Matrix_<Cell_> shape;
        WeightedRiskResult_Get_Shape(result, &shape);
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

struct XlRegister_WeightedRiskResult_Get_Shape_
{
    XlRegister_WeightedRiskResult_Get_Shape_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Completed result");
        Excel::Register("Base", "xl_WeightedRiskResult_Get_Shape", "WEIGHTEDRISKRESULT.GET.SHAPE", "Extract the exact gradient dimensions", "QQ", "result", argHelp, false);
    }
};
static XlRegister_WeightedRiskResult_Get_Shape_ The_WeightedRiskResult_Get_Shape_XlRegisterer;
