

extern "C" __declspec(dllexport) OPER_* xl_Calibrate_MultiCurve
    (const OPER_* xl_stages, const OPER_* xl_recordQuoteRisk)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("Calibrate_MultiCurve");        
        argName = "stages (input #1)";
        const Vector_<Handle_<Storable_>> stages = Excel::ToHandleBaseVector(_env, xl_stages);
        argName = "recordQuoteRisk (input #2)";
        const bool recordQuoteRisk = Excel::ToBool(xl_recordQuoteRisk, true).value_or(false);
        argName = 0;
		Handle_<StorableMultiCurveCalibrationResult_> result;
        Calibrate_MultiCurve(stages, recordQuoteRisk, &result);
        Excel::Retval_ retval;
        retval.Load(_env, result);
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

struct XlRegister_Calibrate_MultiCurve_
{
    XlRegister_Calibrate_MultiCurve_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("CURVECALIBRATIONSTAGE.NEW handles in calibration order");
        argHelp.push_back("TRUE captures the discount and forward stages for quote risk");
        Excel::Register("Base", "xl_Calibrate_MultiCurve", "CALIBRATE.MULTICURVE", "Fit a sequential same-currency curve bundle", "QQQ", "stages,[recordQuoteRisk]", argHelp, false);
    }
};
static XlRegister_Calibrate_MultiCurve_ The_Calibrate_MultiCurve_XlRegisterer;
