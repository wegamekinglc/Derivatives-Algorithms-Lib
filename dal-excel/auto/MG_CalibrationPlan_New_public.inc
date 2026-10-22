

extern "C" __declspec(dllexport) OPER_* xl_CalibrationPlan_New
    (const OPER_* xl_instrument, const OPER_* xl_measureId, const OPER_* xl_context, const OPER_* xl_options)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CalibrationPlan_New");        
        argName = "instrument (input #1)";
        const Handle_<CalibrationInstrument_> instrument = Excel::ToHandle<CalibrationInstrument_>(_env, xl_instrument);
        argName = "measureId (input #2)";
        const String_ measureId = Excel::ToString(xl_measureId);
        argName = "measureId"; Excel::ValidateNativeTextInput(xl_measureId, "/measureId", "InvalidCalibrationInput");
        argName = "context (input #3)";
        const Handle_<StorableCalibrationPreparationContext_> context = Excel::ToHandle<StorableCalibrationPreparationContext_>(_env, xl_context);
        argName = "options (input #4)";
        const Handle_<StorableCalibrationEvaluationOptions_> options = Excel::ToHandle<StorableCalibrationEvaluationOptions_>(_env, xl_options, true);
        argName = 0;
		Handle_<StorablePreparedCalibrationInstrument_> plan;
        CalibrationPlan_New(instrument, measureId, context, options, &plan);
        Excel::Retval_ retval;
        retval.Load(_env, plan);
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

struct XlRegister_CalibrationPlan_New_
{
    XlRegister_CalibrationPlan_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Typed instrument");
        argHelp.push_back("Typed measureId");
        argHelp.push_back("Typed context");
        argHelp.push_back("Typed options");
        Excel::Register("Base", "xl_CalibrationPlan_New", "CALIBRATIONPLAN.NEW", "Owning typed script calibration interface", "QQQQQ", "instrument,measureId,context,[options]", argHelp, false);
    }
};
static XlRegister_CalibrationPlan_New_ The_CalibrationPlan_New_XlRegisterer;
