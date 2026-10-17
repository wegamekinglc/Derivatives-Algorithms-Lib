

extern "C" __declspec(dllexport) OPER_* xl_PortfolioMonteCarlo_ValueWithWeightedRisk
    (const OPER_* xl_portfolio, const OPER_* xl_n_paths, const OPER_* xl_request, const OPER_* xl_valuation, const OPER_* xl_simulation)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("PortfolioMonteCarlo_ValueWithWeightedRisk");        
        argName = "portfolio (input #1)";
        const Handle_<ScriptPortfolioData_> portfolio = Excel::ToHandle<ScriptPortfolioData_>(_env, xl_portfolio);
        xl_portfolio = Excel::ScriptScalarInput(xl_portfolio); argName = "n_paths"; Excel::ValidateRiskRequestPaths(xl_n_paths, "PortfolioMonteCarlo_ValueWithWeightedRisk");
        argName = "n_paths (input #2)";
        const double n_paths = Excel::ToDouble(xl_n_paths);
        xl_request = Excel::ScriptScalarInput(xl_request); xl_valuation = Excel::ScriptScalarInput(xl_valuation); xl_simulation = Excel::ScriptScalarInput(xl_simulation);
        argName = "request (input #3)";
        const Handle_<StorablePortfolioWeightedRiskRequest_> request = Excel::ToHandle<StorablePortfolioWeightedRiskRequest_>(_env, xl_request, true);
        argName = "valuation (input #4)";
        const Handle_<StorableScriptValuationSettings_> valuation = Excel::ToHandle<StorableScriptValuationSettings_>(_env, xl_valuation, true);
        argName = "simulation (input #5)";
        const Handle_<StorableMonteCarloSettings_> simulation = Excel::ToHandle<StorableMonteCarloSettings_>(_env, xl_simulation, true);
        argName = 0;
		Handle_<StorablePortfolioWeightedRiskResult_> result;
        PortfolioMonteCarlo_ValueWithWeightedRisk(portfolio, n_paths, request, valuation, simulation, &result);
        Excel::Retval_ retval;
        retval.Load(_env, result);
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

struct XlRegister_PortfolioMonteCarlo_ValueWithWeightedRisk_
{
    XlRegister_PortfolioMonteCarlo_ValueWithWeightedRisk_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Sealed portfolio");
        argHelp.push_back("Positive integer paths, excluding bool and text, at most INT_MAX");
        argHelp.push_back("Blank selects every payoff with unit weights and all native columns");
        argHelp.push_back("Blank captures current date and required history once");
        argHelp.push_back("Blank enables native AAD; passive mode has zero risk columns");
        Excel::Register("Base", "xl_PortfolioMonteCarlo_ValueWithWeightedRisk", "PORTFOLIOMONTECARLO.VALUEWITHWEIGHTEDRISK", "Value a sealed portfolio and one global weighted gradient", "QQQQQQ", "portfolio,n_paths,[request],[valuation],[simulation]", argHelp, false);
    }
};
static XlRegister_PortfolioMonteCarlo_ValueWithWeightedRisk_ The_PortfolioMonteCarlo_ValueWithWeightedRisk_XlRegisterer;
