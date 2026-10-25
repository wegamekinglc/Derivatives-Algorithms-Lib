

extern "C" __declspec(dllexport) OPER_* xl_XccyInstrument_ValueByGaussian
    (const OPER_* xl_instrument, const OPER_* xl_model, const OPER_* xl_measureId, const OPER_* xl_fixings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("XccyInstrument_ValueByGaussian");        
        argName = "instrument (input #1)";
        const Handle_<CalibrationInstrument_> instrument = Excel::ToHandle<CalibrationInstrument_>(_env, xl_instrument);
        argName = "model (input #2)";
        const Handle_<XccyGaussianModelData_> model = Excel::ToHandle<XccyGaussianModelData_>(_env, xl_model);
        argName = "measureId (input #3)";
        const String_ measureId = Excel::ToString(xl_measureId);
        argName = "fixings (input #4)";
        const Handle_<StorableMarketFixingSnapshot_> fixings = Excel::ToHandle<StorableMarketFixingSnapshot_>(_env, xl_fixings, true);
        argName = 0;
		Handle_<StorableCalibrationOutcome_> outcome;
        XccyInstrument_ValueByGaussian(instrument, model, measureId, fixings, &outcome);
        Excel::Retval_ retval;
        retval.Load(_env, outcome);
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

struct XlRegister_XccyInstrument_ValueByGaussian_
{
    XlRegister_XccyInstrument_ValueByGaussian_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Shared immutable instrument");
        argHelp.push_back("Explicit Gaussian model");
        argHelp.push_back("Price or BasisSpread");
        argHelp.push_back("Omitted captures history; explicit empty remains authoritative");
        Excel::Register("Base", "xl_XccyInstrument_ValueByGaussian", "XCCYINSTRUMENT.VALUEBYGAUSSIAN", "Evaluate the same immutable XCCY source with Gaussian moments", "QQQQQ", "instrument,model,measureId,[fixings]", argHelp, false);
    }
};
static XlRegister_XccyInstrument_ValueByGaussian_ The_XccyInstrument_ValueByGaussian_XlRegisterer;
