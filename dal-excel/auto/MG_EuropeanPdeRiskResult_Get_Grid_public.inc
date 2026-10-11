

extern "C" __declspec(dllexport) OPER_* xl_EuropeanPdeRiskResult_Get_Grid
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("EuropeanPdeRiskResult_Get_Grid");        
        argName = "result (input #1)";
        const Handle_<StorableEuropeanPdeRiskResult_> result = Excel::ToHandle<StorableEuropeanPdeRiskResult_>(_env, xl_result);
        argName = 0;
		Matrix_<Cell_> grid;
        EuropeanPdeRiskResult_Get_Grid(result, &grid);
        Excel::Retval_ retval;
        retval.Load(grid);
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

struct XlRegister_EuropeanPdeRiskResult_Get_Grid_
{
    XlRegister_EuropeanPdeRiskResult_Get_Grid_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Completed owning result");
        Excel::Register("Base", "xl_EuropeanPdeRiskResult_Get_Grid", "EUROPEANPDERISKRESULT.GET.GRID", "Copy the fixed physical European spot grid", "QQ", "result", argHelp, false);
    }
};
static XlRegister_EuropeanPdeRiskResult_Get_Grid_ The_EuropeanPdeRiskResult_Get_Grid_XlRegisterer;
