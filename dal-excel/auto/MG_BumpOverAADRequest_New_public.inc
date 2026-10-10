

extern "C" __declspec(dllexport) OPER_* xl_BumpOverAADRequest_New
    (const OPER_* xl_name, const OPER_* xl_directions, const OPER_* xl_steps, const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("BumpOverAADRequest_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "name"; Excel::ValidateRiskRequestText(xl_name, "BumpOverAADRequest_New; name"); argName = "directions"; Excel::ValidateCurvatureNumbers(xl_directions, "BumpOverAADRequest_New", "directions"); const Excel::ScriptSettingsInput_ normalizedDirections(xl_directions); xl_directions = normalizedDirections.Get(); argName = "steps"; Excel::ValidateCurvatureNumbers(xl_steps, "BumpOverAADRequest_New", "steps"); const Excel::ScriptSettingsInput_ normalizedSteps(xl_steps); xl_steps = normalizedSteps.Get(); const Excel::ScriptSettingsInput_ normalizedSettings(xl_settings); xl_settings = normalizedSettings.Get(); argName = "settings"; Excel::ValidateRiskRequestSettings(xl_settings, "BumpOverAADRequest_New");
        argName = "directions (input #2)";
        const Matrix_<Cell_> directions = Excel::ToCellMatrix(xl_directions, true, true);
        argName = "steps (input #3)";
        const Matrix_<Cell_> steps = Excel::ToCellMatrix(xl_steps, true, true);
        argName = "settings (input #4)";
        const Matrix_<Cell_> settings = Excel::ToCellMatrix(xl_settings, true, true);
        argName = 0;
		Handle_<StorableBumpOverAADRequest_> request;
        BumpOverAADRequest_New(name, directions, steps, settings, &request);
        Excel::Retval_ retval;
        retval.Load(_env, request);
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

struct XlRegister_BumpOverAADRequest_New_
{
    XlRegister_BumpOverAADRequest_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Object name");
        argHelp.push_back("Finite direction rows; blank means zero directions with input_count from settings");
        argHelp.push_back("One finite positive step per direction; row or column vector; blank for zero directions");
        argHelp.push_back("input_count and optional native budgets; blank caps unset, zero caps actual; worksheet copies excluded");
        Excel::Register("Base", "xl_BumpOverAADRequest_New", "BUMPOVERAADREQUEST.NEW", "Create an owning finite-step curvature direction request", "QQQQQ", "name,[directions],[steps],[settings]", argHelp, false);
    }
};
static XlRegister_BumpOverAADRequest_New_ The_BumpOverAADRequest_New_XlRegisterer;
