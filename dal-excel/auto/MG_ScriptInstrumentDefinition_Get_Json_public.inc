

extern "C" __declspec(dllexport) OPER_* xl_ScriptInstrumentDefinition_Get_Json
    (const OPER_* xl_definition)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("ScriptInstrumentDefinition_Get_Json");        
        argName = "definition (input #1)";
        const Handle_<ScriptInstrumentDefinition_> definition = Excel::ToHandle<ScriptInstrumentDefinition_>(_env, xl_definition);
        argName = 0;
		Vector_<String_> json;
        ScriptInstrumentDefinition_Get_Json(definition, &json);
        Excel::Retval_ retval;
        retval.Load(json);
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

struct XlRegister_ScriptInstrumentDefinition_Get_Json_
{
    XlRegister_ScriptInstrumentDefinition_Get_Json_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Typed definition");
        Excel::Register("Base", "xl_ScriptInstrumentDefinition_Get_Json", "SCRIPTINSTRUMENTDEFINITION.GET.JSON", "Owning typed script calibration interface", "QQ", "definition", argHelp, false);
    }
};
static XlRegister_ScriptInstrumentDefinition_Get_Json_ The_ScriptInstrumentDefinition_Get_Json_XlRegisterer;
