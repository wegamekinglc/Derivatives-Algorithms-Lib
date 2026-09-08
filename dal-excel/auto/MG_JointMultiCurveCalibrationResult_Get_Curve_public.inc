

extern "C" __declspec(dllexport) OPER_* xl_JointMultiCurveCalibrationResult_Get_Curve
    (const OPER_* xl_result, const OPER_* xl_curveIndex)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("JointMultiCurveCalibrationResult_Get_Curve");        
        argName = "result (input #1)";
        const Handle_<StorableJointMultiCurveCalibrationResult_> result = Excel::ToHandle<StorableJointMultiCurveCalibrationResult_>(_env, xl_result);
        argName = "curveIndex (input #2)";
        const int curveIndex = Excel::ToInt(xl_curveIndex);
        argName = 0;
		Handle_<StorableDiscountCurve_> curve;
        JointMultiCurveCalibrationResult_Get_Curve(result, curveIndex, &curve);
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

struct XlRegister_JointMultiCurveCalibrationResult_Get_Curve_
{
    XlRegister_JointMultiCurveCalibrationResult_Get_Curve_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Generic joint calibration result");
        argHelp.push_back("Zero-based declaration index, matching curve:0, curve:1, and so on");
        Excel::Register("Base", "xl_JointMultiCurveCalibrationResult_Get_Curve", "JOINTMULTICURVECALIBRATIONRESULT.GET.CURVE", "Extract one calibrated discount or forward curve by declaration index", "QQQ", "result,curveIndex", argHelp, false);
    }
};
static XlRegister_JointMultiCurveCalibrationResult_Get_Curve_ The_JointMultiCurveCalibrationResult_Get_Curve_XlRegisterer;
