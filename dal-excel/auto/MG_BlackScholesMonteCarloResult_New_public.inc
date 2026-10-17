

extern "C" __declspec(dllexport) OPER_* xl_BlackScholesMonteCarloResult_New
    (const OPER_* xl_name, const OPER_* xl_plan, const OPER_* xl_point, const OPER_* xl_paths, const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("BlackScholesMonteCarloResult_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "name"; Excel::ValidateRiskRequestText(xl_name, "BlackScholesMonteCarloResult_New; name"); argName = "plan"; Excel::ValidateRiskRequestText(xl_plan, "BlackScholesMonteCarloResult_New; plan"); xl_plan = Excel::ScriptScalarInput(xl_plan); argName = "settings"; Excel::ValidateRiskRequestText(xl_settings, "BlackScholesMonteCarloResult_New; settings"); xl_settings = Excel::ScriptScalarInput(xl_settings); argName = "point"; Excel::ValidateCurvatureNumbers(xl_point, "BlackScholesMonteCarloResult_New", "point"); const Excel::ScriptSettingsInput_ normalizedPoint(xl_point); xl_point = normalizedPoint.Get(); argName = "paths"; Excel::ValidateCurvatureNumber(*Excel::ScriptScalarInput(xl_paths), "BlackScholesMonteCarloResult_New; paths; "); xl_paths = Excel::ScriptScalarInput(xl_paths); const Excel::ScriptSettingsInput_ normalizedPaths(xl_paths); xl_paths = normalizedPaths.Get();
        argName = "plan (input #2)";
        const Handle_<StorableBlackScholesMonteCarloPlan_> plan = Excel::ToHandle<StorableBlackScholesMonteCarloPlan_>(_env, xl_plan);
        argName = "point (input #3)";
        const Matrix_<Cell_> point = Excel::ToCellMatrix(xl_point);
        argName = "paths (input #4)";
        const double paths = Excel::ToDouble(xl_paths);
        argName = "settings (input #5)";
        const Handle_<StorableSegmentedMonteCarloSettings_> settings = Excel::ToHandle<StorableSegmentedMonteCarloSettings_>(_env, xl_settings, true);
        argName = 0;
		Handle_<StorableBlackScholesMonteCarloResult_> result;
        BlackScholesMonteCarloResult_New(name, plan, point, paths, settings, &result);
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

struct XlRegister_BlackScholesMonteCarloResult_New_
{
    XlRegister_BlackScholesMonteCarloResult_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Object name");
        argHelp.push_back("Sealed native Black-Scholes script plan");
        argHelp.push_back("Complete finite numeric row or column; blank gaps reject");
        argHelp.push_back("Positive exact size_t integer at most 2^53-1");
        argHelp.push_back("Copied segmented settings; blank uses native defaults");
        Excel::Register("Base", "xl_BlackScholesMonteCarloResult_New", "BLACKSCHOLESMONTECARLORESULT.NEW", "Evaluate an owning segmented native MC mean and full raw gradient", "QQQQQQ", "name,plan,point,paths,[settings]", argHelp, false);
    }
};
static XlRegister_BlackScholesMonteCarloResult_New_ The_BlackScholesMonteCarloResult_New_XlRegisterer;
