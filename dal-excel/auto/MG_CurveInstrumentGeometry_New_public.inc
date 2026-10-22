

extern "C" __declspec(dllexport) OPER_* xl_CurveInstrumentGeometry_New
    (const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CurveInstrumentGeometry_New");        
        argName = "settings (input #1)";
        const Matrix_<Cell_> settings = Excel::ToCellMatrix(xl_settings);
        const Excel::ScriptSettingsInput_ normalized_settings(xl_settings); xl_settings = normalized_settings.Get(); argName = "settings"; Excel::ValidateScriptSettingsRange(xl_settings, "CurveInstrumentGeometry_New", "settings");
        argName = 0;
		Handle_<StorableCurveInstrumentGeometry_> value;
        CurveInstrumentGeometry_New(settings, &value);
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

struct XlRegister_CurveInstrumentGeometry_New_
{
    XlRegister_CurveInstrumentGeometry_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Exact field/value rows; nested values are typed handles");
        Excel::Register("Base", "xl_CurveInstrumentGeometry_New", "CURVEINSTRUMENTGEOMETRY.NEW", "Create immutable typed calibration settings", "QQ", "settings", argHelp, false);
    }
};
static XlRegister_CurveInstrumentGeometry_New_ The_CurveInstrumentGeometry_New_XlRegisterer;
