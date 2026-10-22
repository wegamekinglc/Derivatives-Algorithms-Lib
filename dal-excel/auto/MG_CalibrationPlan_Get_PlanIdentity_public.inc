

extern "C" __declspec(dllexport) OPER_* xl_CalibrationPlan_Get_PlanIdentity
    (const OPER_* xl_plan)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CalibrationPlan_Get_PlanIdentity");        
        argName = "plan (input #1)";
        const Handle_<StorablePreparedCalibrationInstrument_> plan = Excel::ToHandle<StorablePreparedCalibrationInstrument_>(_env, xl_plan);
        argName = 0;
		String_ value;
        CalibrationPlan_Get_PlanIdentity(plan, &value);
        Excel::Retval_ retval;
        retval.Load(value);
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

struct XlRegister_CalibrationPlan_Get_PlanIdentity_
{
    XlRegister_CalibrationPlan_Get_PlanIdentity_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Prepared instrument plan");
        Excel::Register("Base", "xl_CalibrationPlan_Get_PlanIdentity", "CALIBRATIONPLAN.GET.PLANIDENTITY", "Inspect an immutable admitted preparation plan", "QQ", "plan", argHelp, false);
    }
};
static XlRegister_CalibrationPlan_Get_PlanIdentity_ The_CalibrationPlan_Get_PlanIdentity_XlRegisterer;
