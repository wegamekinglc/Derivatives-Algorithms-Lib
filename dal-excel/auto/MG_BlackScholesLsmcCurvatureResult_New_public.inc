

extern "C" __declspec(dllexport) OPER_* xl_BlackScholesLsmcCurvatureResult_New
    (const OPER_* xl_name, const OPER_* xl_plan, const OPER_* xl_point, const OPER_* xl_paths, const OPER_* xl_bumps)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("BlackScholesLsmcCurvatureResult_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "name"; Excel::ValidateRiskRequestText(xl_name, "BlackScholesLsmcCurvatureResult_New; name"); argName = "plan"; Excel::ValidateRiskRequestText(xl_plan, "BlackScholesLsmcCurvatureResult_New; plan"); xl_plan = Excel::ScriptScalarInput(xl_plan); argName = "bumps"; Excel::ValidateRiskRequestText(xl_bumps, "BlackScholesLsmcCurvatureResult_New; bumps"); xl_bumps = Excel::ScriptScalarInput(xl_bumps); argName = "point"; Excel::ValidateCurvatureNumbers(xl_point, "BlackScholesLsmcCurvatureResult_New", "point"); const Excel::ScriptSettingsInput_ normalizedPoint(xl_point); xl_point = normalizedPoint.Get(); argName = "paths"; Excel::ValidateCurvatureNumber(*Excel::ScriptScalarInput(xl_paths), "BlackScholesLsmcCurvatureResult_New; paths; "); xl_paths = Excel::ScriptScalarInput(xl_paths); const Excel::ScriptSettingsInput_ normalizedpaths(xl_paths); xl_paths = normalizedpaths.Get();
        argName = "plan (input #2)";
        const Handle_<StorableBlackScholesLsmcPlan_> plan = Excel::ToHandle<StorableBlackScholesLsmcPlan_>(_env, xl_plan);
        argName = "point (input #3)";
        const Matrix_<Cell_> point = Excel::ToCellMatrix(xl_point);
        argName = "paths (input #4)";
        const double paths = Excel::ToDouble(xl_paths);
        argName = "bumps (input #5)";
        const Handle_<StorableBumpOverAADRequest_> bumps = Excel::ToHandle<StorableBumpOverAADRequest_>(_env, xl_bumps);
        argName = 0;
		Handle_<StorableBlackScholesLsmcCurvatureResult_> result;
        BlackScholesLsmcCurvatureResult_New(name, plan, point, paths, bumps, &result);
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

struct XlRegister_BlackScholesLsmcCurvatureResult_New_
{
    XlRegister_BlackScholesLsmcCurvatureResult_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Object name");
        argHelp.push_back("Sealed native LSMC plan with AAD enabled");
        argHelp.push_back("Complete finite numeric row or column; blank gaps reject");
        argHelp.push_back("Positive exact size_t integer; combined Sobol blocks must fit native limits");
        argHelp.push_back("Full signed directions, positive actual steps and separate numeric and recording caps");
        Excel::Register("Base", "xl_BlackScholesLsmcCurvatureResult_New", "BLACKSCHOLESLSMCCURVATURERESULT.NEW", "Evaluate finite-step LSMC curvature under Frozen or RetrainedBump policies", "QQQQQQ", "name,plan,point,paths,bumps", argHelp, false);
    }
};
static XlRegister_BlackScholesLsmcCurvatureResult_New_ The_BlackScholesLsmcCurvatureResult_New_XlRegisterer;
