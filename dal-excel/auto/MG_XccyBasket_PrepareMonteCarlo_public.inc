

extern "C" __declspec(dllexport) OPER_* xl_XccyBasket_PrepareMonteCarlo
    (const OPER_* xl_members, const OPER_* xl_model, const OPER_* xl_request)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("XccyBasket_PrepareMonteCarlo");        
        argName = "members (input #1)";
        const Handle_<StorableXccyMonteCarloBasketMembers_> members = Excel::ToHandle<StorableXccyMonteCarloBasketMembers_>(_env, xl_members);
        argName = "model (input #2)";
        const Handle_<XccyGaussianModelData_> model = Excel::ToHandle<XccyGaussianModelData_>(_env, xl_model);
        argName = "request (input #3)";
        const Handle_<StorableXccyMonteCarloBasketRequest_> request = Excel::ToHandle<StorableXccyMonteCarloBasketRequest_>(_env, xl_request);
        argName = 0;
		Handle_<StorableXccyMonteCarloBasketOutcome_> outcome;
        XccyBasket_PrepareMonteCarlo(members, model, request, &outcome);
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

struct XlRegister_XccyBasket_PrepareMonteCarlo_
{
    XlRegister_XccyBasket_PrepareMonteCarlo_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Ordered members");
        argHelp.push_back("Explicit immutable model");
        argHelp.push_back("Explicit phase, sampling and snapshot");
        Excel::Register("Base", "xl_XccyBasket_PrepareMonteCarlo", "XCCYBASKET.PREPAREMONTECARLO", "Prepare all basket members before drawing", "QQQQ", "members,model,request", argHelp, false);
    }
};
static XlRegister_XccyBasket_PrepareMonteCarlo_ The_XccyBasket_PrepareMonteCarlo_XlRegisterer;
