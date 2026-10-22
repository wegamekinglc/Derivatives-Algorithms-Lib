

extern "C" __declspec(dllexport) OPER_* xl_CalibrationOutcome_Get_Curve
    (const OPER_* xl_outcome)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CalibrationOutcome_Get_Curve");        
        argName = "outcome (input #1)";
        const Handle_<StorableCalibrationOutcome_> outcome = Excel::ToHandle<StorableCalibrationOutcome_>(_env, xl_outcome);
        argName = 0;
		Handle_<StorableDiscountCurve_> curve;
        CalibrationOutcome_Get_Curve(outcome, &curve);
        Excel::Retval_ retval;
        retval.Load(_env, curve);
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

struct XlRegister_CalibrationOutcome_Get_Curve_
{
    XlRegister_CalibrationOutcome_Get_Curve_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Typed outcome");
        Excel::Register("Base", "xl_CalibrationOutcome_Get_Curve", "CALIBRATIONOUTCOME.GET.CURVE", "Owning typed script calibration interface", "QQ", "outcome", argHelp, false);
    }
};
static XlRegister_CalibrationOutcome_Get_Curve_ The_CalibrationOutcome_Get_Curve_XlRegisterer;
