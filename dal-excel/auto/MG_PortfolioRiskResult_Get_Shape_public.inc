

extern "C" __declspec(dllexport) OPER_* xl_PortfolioRiskResult_Get_Shape
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("PortfolioRiskResult_Get_Shape");        
        argName = "result (input #1)";
        const Handle_<Storable_> result = Excel::ToHandle<Storable_>(_env, xl_result);
        argName = 0;
		Matrix_<Cell_> shape;
        PortfolioRiskResult_Get_Shape(result, &shape);
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

struct XlRegister_PortfolioRiskResult_Get_Shape_
{
    XlRegister_PortfolioRiskResult_Get_Shape_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Completed weighted or attribution portfolio result");
        Excel::Register("Base", "xl_PortfolioRiskResult_Get_Shape", "PORTFOLIORISKRESULT.GET.SHAPE", "Extract exact risk dimensions without copying risk payloads", "QQ", "result", argHelp, false);
    }
};
static XlRegister_PortfolioRiskResult_Get_Shape_ The_PortfolioRiskResult_Get_Shape_XlRegisterer;
