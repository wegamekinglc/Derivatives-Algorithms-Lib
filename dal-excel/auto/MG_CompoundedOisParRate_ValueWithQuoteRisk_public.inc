

extern "C" __declspec(dllexport) OPER_* xl_CompoundedOisParRate_ValueWithQuoteRisk
    (const OPER_* xl_instrument, const OPER_* xl_tables, const OPER_* xl_snapshot, const OPER_* xl_context, const OPER_* xl_quoteIds)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CompoundedOisParRate_ValueWithQuoteRisk");        
        argName = "instrument (input #1)";
        const Handle_<CalibrationInstrument_> instrument = Excel::ToHandle<CalibrationInstrument_>(_env, xl_instrument);
        argName = "tables (input #2)";
        const Handle_<StorablePreparedCompoundedOis_> tables = Excel::ToHandle<StorablePreparedCompoundedOis_>(_env, xl_tables);
        argName = "snapshot (input #3)";
        const Handle_<StorableScriptCurveQuoteRiskSnapshot_> snapshot = Excel::ToHandle<StorableScriptCurveQuoteRiskSnapshot_>(_env, xl_snapshot);
        argName = "context (input #4)";
        const Handle_<StorableCurveInstrumentCalibrationSpec_> context = Excel::ToHandle<StorableCurveInstrumentCalibrationSpec_>(_env, xl_context);
        argName = "quoteIds (input #5)";
        const Handle_<StorableScriptCurveQuoteIds_> quoteIds = Excel::ToHandle<StorableScriptCurveQuoteIds_>(_env, xl_quoteIds, true);
        argName = 0;
		Handle_<StorableCompoundedOisParRateRiskResult_> value;
        CompoundedOisParRate_ValueWithQuoteRisk(instrument, tables, snapshot, context, quoteIds, &value);
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

struct XlRegister_CompoundedOisParRate_ValueWithQuoteRisk_
{
    XlRegister_CompoundedOisParRate_ValueWithQuoteRisk_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Registered bound OIS instrument");
        argHelp.push_back("Frozen OIS schedule");
        argHelp.push_back("Owning fitted snapshot");
        argHelp.push_back("Current calibration context");
        argHelp.push_back("Selected quote IDs in output order; omit for the snapshot axis");
        Excel::Register("Base", "xl_CompoundedOisParRate_ValueWithQuoteRisk", "COMPOUNDEDOISPARRATE.VALUEWITHQUOTERISK", "Value a registered compounded OIS par rate and its signed calibration quote sensitivities", "QQQQQQ", "instrument,tables,snapshot,context,[quoteIds]", argHelp, false);
    }
};
static XlRegister_CompoundedOisParRate_ValueWithQuoteRisk_ The_CompoundedOisParRate_ValueWithQuoteRisk_XlRegisterer;
