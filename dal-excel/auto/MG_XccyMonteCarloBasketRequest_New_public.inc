

extern "C" __declspec(dllexport) OPER_* xl_XccyMonteCarloBasketRequest_New
    (const OPER_* xl_phase, const OPER_* xl_sampling, const OPER_* xl_fixings, const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("XccyMonteCarloBasketRequest_New");        
        argName = "phase (input #1)";
        const Cell_ phase = Excel::ToCell_orXccyBasketPhase(xl_phase, false);
        argName = "sampling (input #2)";
        const Handle_<StorableCalibrationMonteCarloSampling_> sampling = Excel::ToHandle<StorableCalibrationMonteCarloSampling_>(_env, xl_sampling);
        argName = "fixings (input #3)";
        const Handle_<StorableMarketFixingSnapshot_> fixings = Excel::ToHandle<StorableMarketFixingSnapshot_>(_env, xl_fixings);
        argName = "settings (input #4)";
        const Matrix_<Cell_> settings = Excel::ToCellMatrix_orXccy(xl_settings, Excel::XccyTableInput_("XccyMonteCarloBasketRequest_New", "settings", 2), true, true);
        argName = 0;
		Handle_<StorableXccyMonteCarloBasketRequest_> value;
        XccyMonteCarloBasketRequest_New(phase, sampling, fixings, settings, &value);
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

struct XlRegister_XccyMonteCarloBasketRequest_New_
{
    XlRegister_XccyMonteCarloBasketRequest_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Fit, Validation or HeldOut");
        argHelp.push_back("Explicit typed sampling");
        argHelp.push_back("Required snapshot, including explicit empty history");
        argHelp.push_back("Optional simulation, derivative_method and parameter_labels");
        Excel::Register("Base", "xl_XccyMonteCarloBasketRequest_New", "XCCYMONTECARLOBASKETREQUEST.NEW", "Create an explicit XCCY basket request", "QQQQQ", "phase,sampling,fixings,[settings]", argHelp, false);
    }
};
static XlRegister_XccyMonteCarloBasketRequest_New_ The_XccyMonteCarloBasketRequest_New_XlRegisterer;
