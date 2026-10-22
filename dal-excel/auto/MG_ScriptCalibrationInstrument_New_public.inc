

extern "C" __declspec(dllexport) OPER_* xl_ScriptCalibrationInstrument_New
    (const OPER_* xl_id, const OPER_* xl_definition, const OPER_* xl_bindings, const OPER_* xl_pillar)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("ScriptCalibrationInstrument_New");        
        argName = "id (input #1)";
        const String_ id = Excel::ToString(xl_id);
        argName = "id"; Excel::ValidateNativeTextInput(xl_id, "/id", "InvalidCalibrationInput");
        argName = "definition (input #2)";
        const Handle_<ScriptInstrumentDefinition_> definition = Excel::ToHandle<ScriptInstrumentDefinition_>(_env, xl_definition);
        argName = "bindings (input #3)";
        const Matrix_<Cell_> bindings = Excel::ToCellMatrix(xl_bindings, true, true);
        const Excel::ScriptSettingsInput_ normalized_bindings(xl_bindings); xl_bindings = normalized_bindings.Get();
        argName = "pillar (input #4)";
        const Cell_ pillar = Excel::ToCell(xl_pillar, true);
        const Excel::ScriptSettingsInput_ normalized_pillar(xl_pillar); xl_pillar = normalized_pillar.Get();
        argName = 0;
		Handle_<CalibrationInstrument_> instrument;
        ScriptCalibrationInstrument_New(id, definition, bindings, pillar, &instrument);
        Excel::Retval_ retval;
        retval.Load(_env, instrument);
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

struct XlRegister_ScriptCalibrationInstrument_New_
{
    XlRegister_ScriptCalibrationInstrument_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Typed id");
        argHelp.push_back("Typed definition");
        argHelp.push_back("Typed bindings");
        argHelp.push_back("Typed pillar");
        Excel::Register("Base", "xl_ScriptCalibrationInstrument_New", "SCRIPTCALIBRATIONINSTRUMENT.NEW", "Owning typed script calibration interface", "QQQQQ", "id,definition,[bindings],[pillar]", argHelp, false);
    }
};
static XlRegister_ScriptCalibrationInstrument_New_ The_ScriptCalibrationInstrument_New_XlRegisterer;
