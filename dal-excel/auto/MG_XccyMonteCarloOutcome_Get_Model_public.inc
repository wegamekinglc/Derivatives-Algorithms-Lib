

extern "C" __declspec(dllexport) OPER_* xl_XccyMonteCarloOutcome_Get_Model
    (const OPER_* xl_outcome)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("XccyMonteCarloOutcome_Get_Model");        
        argName = "outcome (input #1)";
        const Handle_<StorableXccyMonteCarloOutcome_> outcome = Excel::ToHandle<StorableXccyMonteCarloOutcome_>(_env, xl_outcome);
        argName = 0;
		Handle_<XccyGaussianModelData_> model;
        XccyMonteCarloOutcome_Get_Model(outcome, &model);
        Excel::Retval_ retval;
        retval.Load(_env, model);
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

struct XlRegister_XccyMonteCarloOutcome_Get_Model_
{
    XlRegister_XccyMonteCarloOutcome_Get_Model_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Typed outcome");
        Excel::Register("Base", "xl_XccyMonteCarloOutcome_Get_Model", "XCCYMONTECARLOOUTCOME.GET.MODEL", "XccyMonteCarloOutcome Get Model", "QQ", "outcome", argHelp, false);
    }
};
static XlRegister_XccyMonteCarloOutcome_Get_Model_ The_XccyMonteCarloOutcome_Get_Model_XlRegisterer;
