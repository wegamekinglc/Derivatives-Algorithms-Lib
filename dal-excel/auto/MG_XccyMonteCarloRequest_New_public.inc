

extern "C" __declspec(dllexport) OPER_* xl_XccyMonteCarloRequest_New
    (const OPER_* xl_measureId, const OPER_* xl_phase, const OPER_* xl_sampling, const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("XccyMonteCarloRequest_New");        
        argName = "measureId (input #1)";
        const String_ measureId = Excel::ToString(xl_measureId);
        argName = "phase"; Excel::ValidateXccyPhaseInput(xl_phase);
        argName = "phase (input #2)";
        const Cell_ phase = Excel::ToCell(xl_phase);
        argName = "sampling (input #3)";
        const Handle_<StorableCalibrationMonteCarloSampling_> sampling = Excel::ToHandle<StorableCalibrationMonteCarloSampling_>(_env, xl_sampling);
        argName = "settings (input #4)";
        const Matrix_<Cell_> settings = Excel::ToCellMatrix_orXccy(xl_settings, Excel::XccyTableInput_("XccyMonteCarloRequest_New", "settings", 2), true, true);
        argName = 0;
		Handle_<StorableXccyMonteCarloRequest_> value;
        XccyMonteCarloRequest_New(measureId, phase, sampling, settings, &value);
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

struct XlRegister_XccyMonteCarloRequest_New_
{
    XlRegister_XccyMonteCarloRequest_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Typed measureId");
        argHelp.push_back("Typed phase");
        argHelp.push_back("Typed sampling");
        argHelp.push_back("Typed settings");
        Excel::Register("Base", "xl_XccyMonteCarloRequest_New", "XCCYMONTECARLOREQUEST.NEW", "XccyMonteCarloRequest New", "QQQQQ", "measureId,phase,sampling,[settings]", argHelp, false);
    }
};
static XlRegister_XccyMonteCarloRequest_New_ The_XccyMonteCarloRequest_New_XlRegisterer;
