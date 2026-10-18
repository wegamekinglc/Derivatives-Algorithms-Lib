

extern "C" __declspec(dllexport) OPER_* xl_EarlyExercisePdeRequest_New
    (const OPER_* xl_name, const OPER_* xl_contract, const OPER_* xl_market, const OPER_* xl_carry, const OPER_* xl_exercise_dates, const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("EarlyExercisePdeRequest_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "name"; Excel::ValidateRiskRequestText(xl_name, "EarlyExercisePdeRequest_New; name"); const Excel::ScriptSettingsInput_ normalized_contract(xl_contract); xl_contract = normalized_contract.Get(); argName = "contract"; Excel::ValidateEarlyExercisePdeRange(xl_contract, "contract"); const Excel::ScriptSettingsInput_ normalized_market(xl_market); xl_market = normalized_market.Get(); argName = "market"; Excel::ValidateEarlyExercisePdeRange(xl_market, "market"); const Excel::ScriptSettingsInput_ normalized_carry(xl_carry); xl_carry = normalized_carry.Get(); argName = "carry"; Excel::ValidateEarlyExercisePdeRange(xl_carry, "carry"); const Excel::ScriptSettingsInput_ normalized_exercise_dates(xl_exercise_dates); xl_exercise_dates = normalized_exercise_dates.Get(); argName = "exercise_dates"; Excel::ValidateEarlyExercisePdeRange(xl_exercise_dates, "exercise_dates"); xl_settings = Excel::ScriptScalarInput(xl_settings);
        argName = "contract (input #2)";
        const Matrix_<Cell_> contract = Excel::ToCellMatrix(xl_contract, false, true);
        argName = "market (input #3)";
        const Matrix_<Cell_> market = Excel::ToCellMatrix(xl_market, false, true);
        argName = "carry (input #4)";
        const Matrix_<Cell_> carry = Excel::ToCellMatrix(xl_carry, false, true);
        argName = "exercise_dates (input #5)";
        const Matrix_<Cell_> exercise_dates = Excel::ToCellMatrix(xl_exercise_dates, true, true);
        argName = "settings (input #6)";
        const Handle_<StorableEarlyExercisePdeSettings_> settings = Excel::ToHandle<StorableEarlyExercisePdeSettings_>(_env, xl_settings, true);
        argName = 0;
		Handle_<StorableEarlyExercisePdeRequest_> request;
        EarlyExercisePdeRequest_New(name, contract, market, carry, exercise_dates, settings, &request);
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

struct XlRegister_EarlyExercisePdeRequest_New_
{
    XlRegister_EarlyExercisePdeRequest_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Object name");
        argHelp.push_back("EvaluationDate and MaturityDate integer Excel serials; Strike; OptionType; ExerciseStyle using snake_case keys");
        argHelp.push_back("Spot and decimal Volatility using snake_case keys");
        argHelp.push_back("EndDate integer Excel serial / Rate / DividendYield; no header");
        argHelp.push_back("Single row or column of integer Excel date serials; required Bermudan; empty American");
        argHelp.push_back("Optional settings handle");
        Excel::Register("Base", "xl_EarlyExercisePdeRequest_New", "EARLYEXERCISEPDEREQUEST.NEW", "Create an owning BS early exercise request", "QQQQQQQ", "name,contract,market,carry,[exercise_dates],[settings]", argHelp, false);
    }
};
static XlRegister_EarlyExercisePdeRequest_New_ The_EarlyExercisePdeRequest_New_XlRegisterer;
