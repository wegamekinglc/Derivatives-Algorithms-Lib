

extern "C" __declspec(dllexport) OPER_* xl_LsmcExercisePolicy_Get_Coefficients
    (const OPER_* xl_policy)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("LsmcExercisePolicy_Get_Coefficients");        
        argName = "policy (input #1)";
        const Handle_<StorableLsmcExercisePolicy_> policy = Excel::ToHandle<StorableLsmcExercisePolicy_>(_env, xl_policy);
        argName = "policy"; Excel::ValidateRiskRequestText(xl_policy, "LsmcExercisePolicy_Get_Coefficients; policy"); xl_policy = Excel::ScriptScalarInput(xl_policy);
        argName = 0;
		Matrix_<Cell_> coefficients;
        LsmcExercisePolicy_Get_Coefficients(policy, &coefficients);
        Excel::Retval_ retval;
        retval.Load(coefficients);
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

struct XlRegister_LsmcExercisePolicy_Get_Coefficients_
{
    XlRegister_LsmcExercisePolicy_Get_Coefficients_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Stored immutable policy");
        Excel::Register("Base", "xl_LsmcExercisePolicy_Get_Coefficients", "LSMCEXERCISEPOLICY.GET.COEFFICIENTS", "Copy LsmcExercisePolicy Get Coefficients", "QQ", "policy", argHelp, false);
    }
};
static XlRegister_LsmcExercisePolicy_Get_Coefficients_ The_LsmcExercisePolicy_Get_Coefficients_XlRegisterer;
