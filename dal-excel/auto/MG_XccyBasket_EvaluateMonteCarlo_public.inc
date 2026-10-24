

extern "C" __declspec(dllexport) OPER_* xl_XccyBasket_EvaluateMonteCarlo
    (const OPER_* xl_prepared, const OPER_* xl_candidate)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("XccyBasket_EvaluateMonteCarlo");        
        argName = "prepared (input #1)";
        const Handle_<StorablePreparedXccyMonteCarloBasket_> prepared = Excel::ToHandle<StorablePreparedXccyMonteCarloBasket_>(_env, xl_prepared);
        argName = "candidate (input #2)";
        const Handle_<XccyGaussianModelData_> candidate = Excel::ToHandle<XccyGaussianModelData_>(_env, xl_candidate, true);
        argName = 0;
		Handle_<StorableXccyMonteCarloBasketOutcome_> outcome;
        XccyBasket_EvaluateMonteCarlo(prepared, candidate, &outcome);
        Excel::Retval_ retval;
        retval.Load(_env, outcome);
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

struct XlRegister_XccyBasket_EvaluateMonteCarlo_
{
    XlRegister_XccyBasket_EvaluateMonteCarlo_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Prepared basket");
        argHelp.push_back("Optional same geometry model");
        Excel::Register("Base", "xl_XccyBasket_EvaluateMonteCarlo", "XCCYBASKET.EVALUATEMONTECARLO", "Evaluate a prepared basket with checked numeric refresh", "QQQ", "prepared,[candidate]", argHelp, false);
    }
};
static XlRegister_XccyBasket_EvaluateMonteCarlo_ The_XccyBasket_EvaluateMonteCarlo_XlRegisterer;
