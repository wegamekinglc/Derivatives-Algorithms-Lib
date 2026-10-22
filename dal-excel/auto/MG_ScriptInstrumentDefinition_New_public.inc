

extern "C" __declspec(dllexport) OPER_* xl_ScriptInstrumentDefinition_New
    (const OPER_* xl_source, const OPER_* xl_header, const OPER_* xl_parameters, const OPER_* xl_measures, const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("ScriptInstrumentDefinition_New");        
        argName = "source (input #1)";
        const Matrix_<Cell_> source = Excel::ToCellMatrix(xl_source, false, true);
        const Excel::ScriptSettingsInput_ normalized_source(xl_source); xl_source = normalized_source.Get();
        argName = "header (input #2)";
        const Matrix_<Cell_> header = Excel::ToCellMatrix(xl_header, false, true);
        const Excel::ScriptSettingsInput_ normalized_header(xl_header); xl_header = normalized_header.Get(); argName = "header"; Excel::ValidateScriptSettingsRange(xl_header, "ScriptInstrumentDefinition_New", "header");
        argName = "parameters (input #3)";
        const Matrix_<Cell_> parameters = Excel::ToCellMatrix(xl_parameters, false, true);
        const Excel::ScriptSettingsInput_ normalized_parameters(xl_parameters); xl_parameters = normalized_parameters.Get();
        argName = "measures (input #4)";
        const Matrix_<Cell_> measures = Excel::ToCellMatrix(xl_measures, false, true);
        const Excel::ScriptSettingsInput_ normalized_measures(xl_measures); xl_measures = normalized_measures.Get();
        argName = "settings (input #5)";
        const Handle_<StorableScriptProductSettings_> settings = Excel::ToHandle<StorableScriptProductSettings_>(_env, xl_settings, true);
        argName = 0;
		Handle_<ScriptInstrumentDefinition_> definition;
        ScriptInstrumentDefinition_New(source, header, parameters, measures, settings, &definition);
        Excel::Retval_ retval;
        retval.Load(_env, definition);
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

struct XlRegister_ScriptInstrumentDefinition_New_
{
    XlRegister_ScriptInstrumentDefinition_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Typed source");
        argHelp.push_back("Typed header");
        argHelp.push_back("Typed parameters");
        argHelp.push_back("Typed measures");
        argHelp.push_back("Typed settings");
        Excel::Register("Base", "xl_ScriptInstrumentDefinition_New", "SCRIPTINSTRUMENTDEFINITION.NEW", "Owning typed script calibration interface", "QQQQQQ", "source,header,parameters,measures,[settings]", argHelp, false);
    }
};
static XlRegister_ScriptInstrumentDefinition_New_ The_ScriptInstrumentDefinition_New_XlRegisterer;
