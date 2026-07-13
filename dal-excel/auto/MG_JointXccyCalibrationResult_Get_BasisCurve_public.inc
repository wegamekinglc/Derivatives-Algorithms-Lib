

extern "C" __declspec(dllexport) OPER_* xl_JointXccyCalibrationResult_Get_BasisCurve
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("JointXccyCalibrationResult_Get_BasisCurve");        
        argName = "result (input #1)";
        const Handle_<StorableJointXccyCalibrationResult_> result = Excel::ToHandle<StorableJointXccyCalibrationResult_>(_env, xl_result);
        argName = 0;
		Handle_<StorableDiscountCurve_> curve;
        JointXccyCalibrationResult_Get_BasisCurve(result, &curve);
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

struct XlRegister_JointXccyCalibrationResult_Get_BasisCurve_
{
    XlRegister_JointXccyCalibrationResult_Get_BasisCurve_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("The joint calibration result");
        Excel::Register("Base", "xl_JointXccyCalibrationResult_Get_BasisCurve", "JOINTXCCYCALIBRATIONRESULT.GET.BASISCURVE", "Extract the solved cross-currency basis curve from a joint XCCY calibration result", "QQ", "result", argHelp, false);
    }
};
static XlRegister_JointXccyCalibrationResult_Get_BasisCurve_ The_JointXccyCalibrationResult_Get_BasisCurve_XlRegisterer;
