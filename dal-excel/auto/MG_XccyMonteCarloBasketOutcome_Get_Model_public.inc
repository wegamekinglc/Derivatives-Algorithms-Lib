

extern "C" __declspec(dllexport) OPER_* xl_XccyMonteCarloBasketOutcome_Get_Model
    (const OPER_* xl_outcome)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("XccyMonteCarloBasketOutcome_Get_Model");        
        argName = "outcome (input #1)";
        const Handle_<StorableXccyMonteCarloBasketOutcome_> outcome = Excel::ToHandle<StorableXccyMonteCarloBasketOutcome_>(_env, xl_outcome);
        argName = 0;
		Handle_<XccyGaussianModelData_> model;
        XccyMonteCarloBasketOutcome_Get_Model(outcome, &model);
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

struct XlRegister_XccyMonteCarloBasketOutcome_Get_Model_
{
    XlRegister_XccyMonteCarloBasketOutcome_Get_Model_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Owning outcome");
        Excel::Register("Base", "xl_XccyMonteCarloBasketOutcome_Get_Model", "XCCYMONTECARLOBASKETOUTCOME.GET.MODEL", "Extract the geometry validated model", "QQ", "outcome", argHelp, false);
    }
};
static XlRegister_XccyMonteCarloBasketOutcome_Get_Model_ The_XccyMonteCarloBasketOutcome_Get_Model_XlRegisterer;
