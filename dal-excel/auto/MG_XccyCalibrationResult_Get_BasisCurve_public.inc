

extern "C" __declspec(dllexport) OPER_* xl_XccyCalibrationResult_Get_BasisCurve
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("XccyCalibrationResult_Get_BasisCurve");        
        argName = "result (input #1)";
        const Handle_<StorableCrossCurrencyCalibrationResult_> result = Excel::ToHandle<StorableCrossCurrencyCalibrationResult_>(_env, xl_result);
        argName = 0;
		Handle_<StorableDiscountCurve_> curve;
        XccyCalibrationResult_Get_BasisCurve(result, &curve);
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

struct XlRegister_XccyCalibrationResult_Get_BasisCurve_
{
    XlRegister_XccyCalibrationResult_Get_BasisCurve_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("The cross-currency calibration result handle (from CALIBRATE.XCCYMARKET)");
        Excel::Register("Base", "xl_XccyCalibrationResult_Get_BasisCurve", "XCCYCALIBRATIONRESULT.GET.BASISCURVE", "Extract the basis discount curve from a cross-currency calibration result", "QQ", "result", argHelp, false);
    }
};
static XlRegister_XccyCalibrationResult_Get_BasisCurve_ The_XccyCalibrationResult_Get_BasisCurve_XlRegisterer;
