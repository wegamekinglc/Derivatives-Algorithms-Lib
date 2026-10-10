

extern "C" __declspec(dllexport) OPER_* xl_RateCalibration_Recalibrate
    (const OPER_* xl_name, const OPER_* xl_calibration, const OPER_* xl_quotes)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("RateCalibration_Recalibrate");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "name"; Excel::ValidateRiskRequestText(xl_name, "RateCalibration_Recalibrate; name"); argName = "quotes"; Excel::ValidateCurvatureNumbers(xl_quotes, "RateCalibration_Recalibrate", "quotes"); const Excel::ScriptSettingsInput_ normalizedNumbers(xl_quotes); xl_quotes = normalizedNumbers.Get();
        argName = "calibration (input #2)";
        const Handle_<StorableRateCalibrationSnapshot_> calibration = Excel::ToHandle<StorableRateCalibrationSnapshot_>(_env, xl_calibration);
        argName = "quotes (input #3)";
        const Matrix_<Cell_> quotes = Excel::ToCellMatrix(xl_quotes);
        argName = 0;
		Handle_<StorableRateCalibrationSnapshot_> rebuilt;
        RateCalibration_Recalibrate(name, calibration, quotes, &rebuilt);
        Excel::Retval_ retval;
        retval.Load(_env, rebuilt);
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

struct XlRegister_RateCalibration_Recalibrate_
{
    XlRegister_RateCalibration_Recalibrate_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Object name");
        argHelp.push_back("Owning sealed calibration snapshot");
        argHelp.push_back("One finite numeric value per complete raw quote; row or column vector");
        Excel::Register("Base", "xl_RateCalibration_Recalibrate", "RATECALIBRATION.RECALIBRATE", "Rebuild a sealed native rate snapshot at complete raw quotes", "QQQQ", "name,calibration,quotes", argHelp, false);
    }
};
static XlRegister_RateCalibration_Recalibrate_ The_RateCalibration_Recalibrate_XlRegisterer;
