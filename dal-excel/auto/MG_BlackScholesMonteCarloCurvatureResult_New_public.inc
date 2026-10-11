

extern "C" __declspec(dllexport) OPER_* xl_BlackScholesMonteCarloCurvatureResult_New
    (const OPER_* xl_name, const OPER_* xl_plan, const OPER_* xl_point, const OPER_* xl_paths, const OPER_* xl_bumps, const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("BlackScholesMonteCarloCurvatureResult_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "name"; Excel::ValidateRiskRequestText(xl_name, "BlackScholesMonteCarloCurvatureResult_New; name"); argName = "plan"; Excel::ValidateRiskRequestText(xl_plan, "BlackScholesMonteCarloCurvatureResult_New; plan"); xl_plan = Excel::ScriptScalarInput(xl_plan); argName = "bumps"; Excel::ValidateRiskRequestText(xl_bumps, "BlackScholesMonteCarloCurvatureResult_New; bumps"); xl_bumps = Excel::ScriptScalarInput(xl_bumps); argName = "settings"; Excel::ValidateRiskRequestText(xl_settings, "BlackScholesMonteCarloCurvatureResult_New; settings"); xl_settings = Excel::ScriptScalarInput(xl_settings); argName = "point"; Excel::ValidateCurvatureNumbers(xl_point, "BlackScholesMonteCarloCurvatureResult_New", "point"); const Excel::ScriptSettingsInput_ normalizedPoint(xl_point); xl_point = normalizedPoint.Get(); argName = "paths"; Excel::ValidateCurvatureNumber(*Excel::ScriptScalarInput(xl_paths), "BlackScholesMonteCarloCurvatureResult_New; paths; "); xl_paths = Excel::ScriptScalarInput(xl_paths); const Excel::ScriptSettingsInput_ normalizedPaths(xl_paths); xl_paths = normalizedPaths.Get();
        argName = "plan (input #2)";
        const Handle_<StorableBlackScholesMonteCarloPlan_> plan = Excel::ToHandle<StorableBlackScholesMonteCarloPlan_>(_env, xl_plan);
        argName = "point (input #3)";
        const Matrix_<Cell_> point = Excel::ToCellMatrix(xl_point);
        argName = "paths (input #4)";
        const double paths = Excel::ToDouble(xl_paths);
        argName = "bumps (input #5)";
        const Handle_<StorableBumpOverAADRequest_> bumps = Excel::ToHandle<StorableBumpOverAADRequest_>(_env, xl_bumps);
        argName = "settings (input #6)";
        const Handle_<StorableSegmentedMonteCarloSettings_> settings = Excel::ToHandle<StorableSegmentedMonteCarloSettings_>(_env, xl_settings, true);
        argName = 0;
		Handle_<StorableBlackScholesMonteCarloCurvatureResult_> result;
        BlackScholesMonteCarloCurvatureResult_New(name, plan, point, paths, bumps, settings, &result);
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

struct XlRegister_BlackScholesMonteCarloCurvatureResult_New_
{
    XlRegister_BlackScholesMonteCarloCurvatureResult_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Object name");
        argHelp.push_back("Sealed native Black-Scholes script plan");
        argHelp.push_back("Complete finite numeric row or column; no blank gaps");
        argHelp.push_back("Positive exact size_t integer at most 2^53-1");
        argHelp.push_back("Full directions and positive actual steps; numeric and recording caps separate");
        argHelp.push_back("Copied segmented settings; blank uses native defaults");
        Excel::Register("Base", "xl_BlackScholesMonteCarloCurvatureResult_New", "BLACKSCHOLESMONTECARLOCURVATURERESULT.NEW", "Evaluate finite-step Hessian products using common absolute native MC paths", "QQQQQQQ", "name,plan,point,paths,bumps,[settings]", argHelp, false);
    }
};
static XlRegister_BlackScholesMonteCarloCurvatureResult_New_ The_BlackScholesMonteCarloCurvatureResult_New_XlRegisterer;
