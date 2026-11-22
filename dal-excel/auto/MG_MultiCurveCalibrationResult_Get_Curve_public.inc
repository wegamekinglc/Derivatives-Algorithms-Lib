

extern "C" __declspec(dllexport) OPER_* xl_MultiCurveCalibrationResult_Get_Curve
    (const OPER_* xl_result, const OPER_* xl_role, const OPER_* xl_route)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("MultiCurveCalibrationResult_Get_Curve");        
        argName = "result (input #1)";
        const Handle_<StorableMultiCurveCalibrationResult_> result = Excel::ToHandle<StorableMultiCurveCalibrationResult_>(_env, xl_result);
        argName = "role (input #2)";
        const String_ role = Excel::ToString(xl_role);
        argName = "route (input #3)";
        const String_ route = Excel::ToString(xl_route);
        argName = 0;
		Handle_<Storable_> curve;
        MultiCurveCalibrationResult_Get_Curve(result, role, route, &curve);
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

struct XlRegister_MultiCurveCalibrationResult_Get_Curve_
{
    XlRegister_MultiCurveCalibrationResult_Get_Curve_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Result of CALIBRATE.MULTICURVE");
        argHelp.push_back("Discount or Forward");
        argHelp.push_back("Collateral type for Discount (e.g. OIS) or tenor for Forward (e.g. 6M)");
        Excel::Register("Base", "xl_MultiCurveCalibrationResult_Get_Curve", "MULTICURVECALIBRATIONRESULT.GET.CURVE", "Extract a fitted curve for use in RATEPRICINGMARKET.NEW", "QQQQ", "result,role,route", argHelp, false);
    }
};
static XlRegister_MultiCurveCalibrationResult_Get_Curve_ The_MultiCurveCalibrationResult_Get_Curve_XlRegisterer;
