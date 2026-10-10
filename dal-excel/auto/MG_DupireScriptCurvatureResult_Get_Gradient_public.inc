

extern "C" __declspec(dllexport) OPER_* xl_DupireScriptCurvatureResult_Get_Gradient
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("DupireScriptCurvatureResult_Get_Gradient");        
        argName = "result (input #1)";
        const Handle_<StorableDupireScriptCurvatureResult_> result = Excel::ToHandle<StorableDupireScriptCurvatureResult_>(_env, xl_result);
        argName = 0;
		Matrix_<Cell_> gradient;
        DupireScriptCurvatureResult_Get_Gradient(result, &gradient);
        Excel::Retval_ retval;
        retval.Load(gradient);
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

struct XlRegister_DupireScriptCurvatureResult_Get_Gradient_
{
    XlRegister_DupireScriptCurvatureResult_Get_Gradient_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Completed owning result");
        Excel::Register("Base", "xl_DupireScriptCurvatureResult_Get_Gradient", "DUPIRESCRIPTCURVATURERESULT.GET.GRADIENT", "Copy the complete raw decimal-vol quote gradient", "QQ", "result", argHelp, false);
    }
};
static XlRegister_DupireScriptCurvatureResult_Get_Gradient_ The_DupireScriptCurvatureResult_Get_Gradient_XlRegisterer;
