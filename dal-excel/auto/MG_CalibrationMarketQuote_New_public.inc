

extern "C" __declspec(dllexport) OPER_* xl_CalibrationMarketQuote_New
    (const OPER_* xl_id, const OPER_* xl_value, const OPER_* xl_unit, const OPER_* xl_convention)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CalibrationMarketQuote_New");        
        argName = "id (input #1)";
        const String_ id = Excel::ToString(xl_id);
        argName = "id"; Excel::ValidateNativeTextInput(xl_id, "/id", "InvalidCalibrationInput");
        argName = "value (input #2)";
        const Cell_ value = Excel::ToCell(xl_value);
        const Excel::ScriptSettingsInput_ normalized_value(xl_value); xl_value = normalized_value.Get();
        argName = "unit (input #3)";
        const String_ unit = Excel::ToString(xl_unit);
        argName = "unit"; Excel::ValidateNativeTextInput(xl_unit, "/unit", "InvalidCalibrationInput");
        argName = "convention (input #4)";
        const Matrix_<Cell_> convention = Excel::ToCellMatrix(xl_convention, true, true);
        const Excel::ScriptSettingsInput_ normalized_convention(xl_convention); xl_convention = normalized_convention.Get(); argName = "convention"; Excel::ValidateScriptSettingsRange(xl_convention, "CalibrationMarketQuote_New", "convention");
        argName = 0;
		Handle_<StorableCalibrationMarketQuote_> quote;
        CalibrationMarketQuote_New(id, value, unit, convention, &quote);
        Excel::Retval_ retval;
        retval.Load(_env, quote);
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

struct XlRegister_CalibrationMarketQuote_New_
{
    XlRegister_CalibrationMarketQuote_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Typed id");
        argHelp.push_back("Typed value");
        argHelp.push_back("Typed unit");
        argHelp.push_back("Typed convention");
        Excel::Register("Base", "xl_CalibrationMarketQuote_New", "CALIBRATIONMARKETQUOTE.NEW", "Owning typed script calibration interface", "QQQQQ", "id,value,unit,[convention]", argHelp, false);
    }
};
static XlRegister_CalibrationMarketQuote_New_ The_CalibrationMarketQuote_New_XlRegisterer;
