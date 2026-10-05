

extern "C" __declspec(dllexport) OPER_* xl_CalibrationQuoteRisk_New
    (const OPER_* xl_name, const OPER_* xl_calibration, const OPER_* xl_parameters, const OPER_* xl_direct)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CalibrationQuoteRisk_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "name"; Excel::ValidateCalibrationTextInput(xl_name, "CalibrationQuoteRisk_New; name");
        argName = "calibration (input #2)";
        const Handle_<StorableCalibrationPullback_> calibration = Excel::ToHandle<StorableCalibrationPullback_>(_env, xl_calibration);
        argName = "parameters (input #3)";
        const Handle_<StorableCalibrationParameterAdjoints_> parameters = Excel::ToHandle<StorableCalibrationParameterAdjoints_>(_env, xl_parameters);
        xl_direct = Excel::ScriptScalarInput(xl_direct);
        argName = "direct (input #4)";
        const Handle_<StorableCalibrationDirectQuoteAdjoints_> direct = Excel::ToHandle<StorableCalibrationDirectQuoteAdjoints_>(_env, xl_direct, true);
        argName = 0;
		Handle_<StorableCalibrationQuoteRisk_> result;
        CalibrationQuoteRisk_New(name, calibration, parameters, direct, &result);
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

struct XlRegister_CalibrationQuoteRisk_New_
{
    XlRegister_CalibrationQuoteRisk_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Object name");
        argHelp.push_back("Common boundary");
        argHelp.push_back("Owned parameter seeds");
        argHelp.push_back("Blank omits the direct contribution");
        Excel::Register("Base", "xl_CalibrationQuoteRisk_New", "CALIBRATIONQUOTERISK.NEW", "Map passive parameter adjoints into separated raw quote derivatives", "QQQQQ", "name,calibration,parameters,[direct]", argHelp, false);
    }
};
static XlRegister_CalibrationQuoteRisk_New_ The_CalibrationQuoteRisk_New_XlRegisterer;
