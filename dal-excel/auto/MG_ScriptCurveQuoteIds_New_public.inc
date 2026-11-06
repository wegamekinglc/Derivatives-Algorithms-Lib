

extern "C" __declspec(dllexport) OPER_* xl_ScriptCurveQuoteIds_New
    (const OPER_* xl_ids)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("ScriptCurveQuoteIds_New");        
        argName = "ids (input #1)";
        const Vector_<Cell_> ids = Excel::ToCellVector(xl_ids);
        const Excel::ScriptSettingsInput_ normalized_ids(xl_ids); xl_ids = normalized_ids.Get();
        argName = 0;
		Handle_<StorableScriptCurveQuoteIds_> value;
        ScriptCurveQuoteIds_New(ids, &value);
        Excel::Retval_ retval;
        retval.Load(_env, value);
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

struct XlRegister_ScriptCurveQuoteIds_New_
{
    XlRegister_ScriptCurveQuoteIds_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Ordered text quote IDs");
        Excel::Register("Base", "xl_ScriptCurveQuoteIds_New", "SCRIPTCURVEQUOTEIDS.NEW", "Preserve case-sensitive quote selection order", "QQ", "ids", argHelp, false);
    }
};
static XlRegister_ScriptCurveQuoteIds_New_ The_ScriptCurveQuoteIds_New_XlRegisterer;
