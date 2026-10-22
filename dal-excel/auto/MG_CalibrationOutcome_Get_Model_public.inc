

extern "C" __declspec(dllexport) OPER_* xl_CalibrationOutcome_Get_Model
    (const OPER_* xl_outcome)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CalibrationOutcome_Get_Model");        
        argName = "outcome (input #1)";
        const Handle_<StorableCalibrationOutcome_> outcome = Excel::ToHandle<StorableCalibrationOutcome_>(_env, xl_outcome);
        argName = 0;
		Handle_<ModelData_> model;
        CalibrationOutcome_Get_Model(outcome, &model);
        Excel::Retval_ retval;
        retval.Load(_env, model);
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

struct XlRegister_CalibrationOutcome_Get_Model_
{
    XlRegister_CalibrationOutcome_Get_Model_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Owning calibration outcome");
        Excel::Register("Base", "xl_CalibrationOutcome_Get_Model", "CALIBRATIONOUTCOME.GET.MODEL", "Return an available GSR model candidate from a calibration outcome", "QQ", "outcome", argHelp, false);
    }
};
static XlRegister_CalibrationOutcome_Get_Model_ The_CalibrationOutcome_Get_Model_XlRegisterer;
