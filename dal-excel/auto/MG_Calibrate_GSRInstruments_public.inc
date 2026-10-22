

extern "C" __declspec(dllexport) OPER_* xl_Calibrate_GSRInstruments
    (const OPER_* xl_initial, const OPER_* xl_observations, const OPER_* xl_parameters, const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("Calibrate_GSRInstruments");        
        argName = "initial (input #1)";
        const Handle_<ModelData_> initial = Excel::ToHandle<ModelData_>(_env, xl_initial);
        argName = "observations (input #2)";
        const Vector_<Handle_<Storable_>> observations = Excel::ToHandleBaseVector(_env, xl_observations);
        argName = "parameters (input #3)";
        const Matrix_<Cell_> parameters = Excel::ToCellMatrix(xl_parameters);
        const Excel::ScriptSettingsInput_ normalized_parameters(xl_parameters); xl_parameters = normalized_parameters.Get();
        argName = "settings (input #4)";
        const Handle_<StorableGSRInstrumentCalibrationSettings_> settings = Excel::ToHandle<StorableGSRInstrumentCalibrationSettings_>(_env, xl_settings);
        argName = 0;
		Handle_<StorableCalibrationOutcome_> outcome;
        Calibrate_GSRInstruments(initial, observations, parameters, settings, &outcome);
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

struct XlRegister_Calibrate_GSRInstruments_
{
    XlRegister_Calibrate_GSRInstruments_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Initial native multi-factor Gaussian model");
        argHelp.push_back("Ordered typed CalibrationObservation handles");
        argHelp.push_back("Exact factor,knot,lower,upper header and numeric rows");
        argHelp.push_back("Required owning domain settings");
        Excel::Register("Base", "xl_Calibrate_GSRInstruments", "CALIBRATE.GSRINSTRUMENTS", "Calibrate selected native g coordinates against bound script Price observations", "QQQQQ", "initial,observations,parameters,settings", argHelp, false);
    }
};
static XlRegister_Calibrate_GSRInstruments_ The_Calibrate_GSRInstruments_XlRegisterer;
