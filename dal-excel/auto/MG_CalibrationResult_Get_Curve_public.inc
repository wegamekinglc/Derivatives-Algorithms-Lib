

extern "C" __declspec(dllexport) OPER_* xl_CalibrationResult_Get_Curve
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CalibrationResult_Get_Curve");        
        argName = "result (input #1)";
        const Handle_<StorableCurveCalibrationResult_> result = Excel::ToHandle<StorableCurveCalibrationResult_>(_env, xl_result);
        argName = 0;
		Handle_<StorableDiscountCurve_> curve;
        CalibrationResult_Get_Curve(result, &curve);
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

struct XlRegister_CalibrationResult_Get_Curve_
{
    XlRegister_CalibrationResult_Get_Curve_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("The calibration result handle (from CALIBRATE.SINGLECURVE)");
        Excel::Register("Base", "xl_CalibrationResult_Get_Curve", "CALIBRATIONRESULT.GET.CURVE", "Extract the calibrated discount curve from a curve calibration result", "QQ", "result", argHelp, false);
    }
};
static XlRegister_CalibrationResult_Get_Curve_ The_CalibrationResult_Get_Curve_XlRegisterer;
