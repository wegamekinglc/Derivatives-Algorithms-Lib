

extern "C" __declspec(dllexport) OPER_* xl_BlackScholesMonteCarloResult_Get_Execution
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("BlackScholesMonteCarloResult_Get_Execution");        
        argName = "result (input #1)";
        const Handle_<StorableBlackScholesMonteCarloResult_> result = Excel::ToHandle<StorableBlackScholesMonteCarloResult_>(_env, xl_result);
        argName = "result"; Excel::ValidateRiskRequestText(xl_result, "BlackScholesMonteCarloResult_Get_Execution; result"); xl_result = Excel::ScriptScalarInput(xl_result);
        argName = 0;
		Matrix_<Cell_> execution;
        BlackScholesMonteCarloResult_Get_Execution(result, &execution);
        Excel::Retval_ retval;
        retval.Load(execution);
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

struct XlRegister_BlackScholesMonteCarloResult_Get_Execution_
{
    XlRegister_BlackScholesMonteCarloResult_Get_Execution_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Completed owning mean and gradient");
        Excel::Register("Base", "xl_BlackScholesMonteCarloResult_Get_Execution", "BLACKSCHOLESMONTECARLORESULT.GET.EXECUTION", "Copy completed segmented MC execution", "QQ", "result", argHelp, false);
    }
};
static XlRegister_BlackScholesMonteCarloResult_Get_Execution_ The_BlackScholesMonteCarloResult_Get_Execution_XlRegisterer;
