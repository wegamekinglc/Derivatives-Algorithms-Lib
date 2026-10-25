

extern "C" __declspec(dllexport) OPER_* xl_XccyGaussianModelSpec_New
    (const OPER_* xl_market, const OPER_* xl_dynamics)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("XccyGaussianModelSpec_New");        
        argName = "market (input #1)";
        const Handle_<StorableXccyGaussianMarketSpec_> market = Excel::ToHandle<StorableXccyGaussianMarketSpec_>(_env, xl_market);
        argName = "dynamics (input #2)";
        const Handle_<StorableXccyGaussianDynamicsSpec_> dynamics = Excel::ToHandle<StorableXccyGaussianDynamicsSpec_>(_env, xl_dynamics);
        argName = 0;
		Handle_<StorableXccyGaussianModelSpec_> value;
        XccyGaussianModelSpec_New(market, dynamics, &value);
        Excel::Retval_ retval;
        retval.Load(_env, value);
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

struct XlRegister_XccyGaussianModelSpec_New_
{
    XlRegister_XccyGaussianModelSpec_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Typed market");
        argHelp.push_back("Typed dynamics");
        Excel::Register("Base", "xl_XccyGaussianModelSpec_New", "XCCYGAUSSIANMODELSPEC.NEW", "XccyGaussianModelSpec New", "QQQ", "market,dynamics", argHelp, false);
    }
};
static XlRegister_XccyGaussianModelSpec_New_ The_XccyGaussianModelSpec_New_XlRegisterer;
