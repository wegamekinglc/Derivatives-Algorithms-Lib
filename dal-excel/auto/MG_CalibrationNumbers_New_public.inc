

extern "C" __declspec(dllexport) OPER_* xl_CalibrationNumbers_New
    (const OPER_* xl_numbers)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CalibrationNumbers_New");        
        argName = "numbers (input #1)";
        const Vector_<Cell_> numbers = Excel::ToCellVector(xl_numbers);
        const Excel::ScriptSettingsInput_ normalized_numbers(xl_numbers); xl_numbers = normalized_numbers.Get();
        argName = 0;
		Handle_<StorableCalibrationNumbers_> value;
        CalibrationNumbers_New(numbers, &value);
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

struct XlRegister_CalibrationNumbers_New_
{
    XlRegister_CalibrationNumbers_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Typed numbers");
        Excel::Register("Base", "xl_CalibrationNumbers_New", "CALIBRATIONNUMBERS.NEW", "Owning typed script calibration interface", "QQ", "numbers", argHelp, false);
    }
};
static XlRegister_CalibrationNumbers_New_ The_CalibrationNumbers_New_XlRegisterer;
