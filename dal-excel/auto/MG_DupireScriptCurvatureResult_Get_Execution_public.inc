

extern "C" __declspec(dllexport) OPER_* xl_DupireScriptCurvatureResult_Get_Execution
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("DupireScriptCurvatureResult_Get_Execution");        
        argName = "result (input #1)";
        const Handle_<StorableDupireScriptCurvatureResult_> result = Excel::ToHandle<StorableDupireScriptCurvatureResult_>(_env, xl_result);
        argName = 0;
		Matrix_<Cell_> execution;
        DupireScriptCurvatureResult_Get_Execution(result, &execution);
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

struct XlRegister_DupireScriptCurvatureResult_Get_Execution_
{
    XlRegister_DupireScriptCurvatureResult_Get_Execution_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Completed owning result");
        Excel::Register("Base", "xl_DupireScriptCurvatureResult_Get_Execution", "DUPIRESCRIPTCURVATURERESULT.GET.EXECUTION", "Copy completed recalibrated Dupire curvature work counters", "QQ", "result", argHelp, false);
    }
};
static XlRegister_DupireScriptCurvatureResult_Get_Execution_ The_DupireScriptCurvatureResult_Get_Execution_XlRegisterer;
