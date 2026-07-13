

extern "C" __declspec(dllexport) OPER_* xl_JointXccyCalibrationResult_Get_ForeignBlock
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("JointXccyCalibrationResult_Get_ForeignBlock");        
        argName = "result (input #1)";
        const Handle_<StorableJointXccyCalibrationResult_> result = Excel::ToHandle<StorableJointXccyCalibrationResult_>(_env, xl_result);
        argName = 0;
		Handle_<StorableCurveBlock_> block;
        JointXccyCalibrationResult_Get_ForeignBlock(result, &block);
        Excel::Retval_ retval;
        retval.Load(_env, block);
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

struct XlRegister_JointXccyCalibrationResult_Get_ForeignBlock_
{
    XlRegister_JointXccyCalibrationResult_Get_ForeignBlock_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("The joint calibration result");
        Excel::Register("Base", "xl_JointXccyCalibrationResult_Get_ForeignBlock", "JOINTXCCYCALIBRATIONRESULT.GET.FOREIGNBLOCK", "Extract the solved foreign curve block from a joint XCCY calibration result", "QQ", "result", argHelp, false);
    }
};
static XlRegister_JointXccyCalibrationResult_Get_ForeignBlock_ The_JointXccyCalibrationResult_Get_ForeignBlock_XlRegisterer;
