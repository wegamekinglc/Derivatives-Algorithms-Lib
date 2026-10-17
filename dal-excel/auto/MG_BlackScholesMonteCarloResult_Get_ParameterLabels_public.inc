

extern "C" __declspec(dllexport) OPER_* xl_BlackScholesMonteCarloResult_Get_ParameterLabels
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("BlackScholesMonteCarloResult_Get_ParameterLabels");        
        argName = "result (input #1)";
        const Handle_<StorableBlackScholesMonteCarloResult_> result = Excel::ToHandle<StorableBlackScholesMonteCarloResult_>(_env, xl_result);
        argName = "result"; Excel::ValidateRiskRequestText(xl_result, "BlackScholesMonteCarloResult_Get_ParameterLabels; result"); xl_result = Excel::ScriptScalarInput(xl_result);
        argName = 0;
		Vector_<String_> parameterlabels;
        BlackScholesMonteCarloResult_Get_ParameterLabels(result, &parameterlabels);
        Excel::Retval_ retval;
        retval.Load(parameterlabels);
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

struct XlRegister_BlackScholesMonteCarloResult_Get_ParameterLabels_
{
    XlRegister_BlackScholesMonteCarloResult_Get_ParameterLabels_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Completed owning mean and gradient");
        Excel::Register("Base", "xl_BlackScholesMonteCarloResult_Get_ParameterLabels", "BLACKSCHOLESMONTECARLORESULT.GET.PARAMETERLABELS", "Copy completed segmented MC parameterlabels", "QQ", "result", argHelp, false);
    }
};
static XlRegister_BlackScholesMonteCarloResult_Get_ParameterLabels_ The_BlackScholesMonteCarloResult_Get_ParameterLabels_XlRegisterer;
