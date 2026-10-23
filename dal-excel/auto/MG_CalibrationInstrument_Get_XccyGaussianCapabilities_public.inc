

extern "C" __declspec(dllexport) OPER_* xl_CalibrationInstrument_Get_XccyGaussianCapabilities
    (const OPER_* xl_instrument, const OPER_* xl_model, const OPER_* xl_request)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CalibrationInstrument_Get_XccyGaussianCapabilities");        
        argName = "instrument (input #1)";
        const Handle_<CalibrationInstrument_> instrument = Excel::ToHandle<CalibrationInstrument_>(_env, xl_instrument);
        argName = "model (input #2)";
        const Handle_<XccyGaussianModelData_> model = Excel::ToHandle<XccyGaussianModelData_>(_env, xl_model);
        argName = "request (input #3)";
        const Handle_<StorableXccyMonteCarloRequest_> request = Excel::ToHandle<StorableXccyMonteCarloRequest_>(_env, xl_request);
        argName = 0;
		Handle_<CalibrationReport_> report;
        CalibrationInstrument_Get_XccyGaussianCapabilities(instrument, model, request, &report);
        Excel::Retval_ retval;
        retval.Load(_env, report);
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

struct XlRegister_CalibrationInstrument_Get_XccyGaussianCapabilities_
{
    XlRegister_CalibrationInstrument_Get_XccyGaussianCapabilities_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Typed instrument");
        argHelp.push_back("Typed model");
        argHelp.push_back("Typed request");
        Excel::Register("Base", "xl_CalibrationInstrument_Get_XccyGaussianCapabilities", "CALIBRATIONINSTRUMENT.GET.XCCYGAUSSIANCAPABILITIES", "CalibrationInstrument Get XccyGaussianCapabilities", "QQQQ", "instrument,model,request", argHelp, false);
    }
};
static XlRegister_CalibrationInstrument_Get_XccyGaussianCapabilities_ The_CalibrationInstrument_Get_XccyGaussianCapabilities_XlRegisterer;
