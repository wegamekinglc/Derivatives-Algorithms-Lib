

extern "C" __declspec(dllexport) OPER_* xl_CalibrationDates_New
    (const OPER_* xl_dates)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CalibrationDates_New");        
        argName = "dates (input #1)";
        const Vector_<Cell_> dates = Excel::ToCellVector(xl_dates);
        const Excel::ScriptSettingsInput_ normalized_dates(xl_dates); xl_dates = normalized_dates.Get();
        argName = 0;
		Handle_<StorableCalibrationDates_> value;
        CalibrationDates_New(dates, &value);
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

struct XlRegister_CalibrationDates_New_
{
    XlRegister_CalibrationDates_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Typed dates");
        Excel::Register("Base", "xl_CalibrationDates_New", "CALIBRATIONDATES.NEW", "Owning typed script calibration interface", "QQ", "dates", argHelp, false);
    }
};
static XlRegister_CalibrationDates_New_ The_CalibrationDates_New_XlRegisterer;
