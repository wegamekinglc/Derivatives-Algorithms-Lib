

extern "C" __declspec(dllexport) OPER_* xl_MonteCarloSettings_Get_Configuration
    (const OPER_* xl_simulation)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("MonteCarloSettings_Get_Configuration");        
        argName = "simulation (input #1)";
        const Handle_<StorableMonteCarloSettings_> simulation = Excel::ToHandle<StorableMonteCarloSettings_>(_env, xl_simulation);
        argName = "simulation"; Excel::ValidateRiskRequestText(xl_simulation, "MonteCarloSettings_Get_Configuration; simulation"); xl_simulation = Excel::ScriptScalarInput(xl_simulation);
        argName = 0;
		Matrix_<Cell_> configuration;
        MonteCarloSettings_Get_Configuration(simulation, &configuration);
        Excel::Retval_ retval;
        retval.Load(configuration);
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

struct XlRegister_MonteCarloSettings_Get_Configuration_
{
    XlRegister_MonteCarloSettings_Get_Configuration_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Stored immutable simulation");
        Excel::Register("Base", "xl_MonteCarloSettings_Get_Configuration", "MONTECARLOSETTINGS.GET.CONFIGURATION", "Copy MonteCarloSettings Get Configuration", "QQ", "simulation", argHelp, false);
    }
};
static XlRegister_MonteCarloSettings_Get_Configuration_ The_MonteCarloSettings_Get_Configuration_XlRegisterer;
