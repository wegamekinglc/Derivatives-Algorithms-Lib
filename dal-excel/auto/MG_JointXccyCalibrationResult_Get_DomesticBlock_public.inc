

extern "C" __declspec(dllexport) OPER_* xl_JointXccyCalibrationResult_Get_DomesticBlock
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("JointXccyCalibrationResult_Get_DomesticBlock");        
        argName = "result (input #1)";
        const Handle_<StorableJointXccyCalibrationResult_> result = Excel::ToHandle<StorableJointXccyCalibrationResult_>(_env, xl_result);
        argName = 0;
		Handle_<StorableCurveBlock_> block;
        JointXccyCalibrationResult_Get_DomesticBlock(result, &block);
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

struct XlRegister_JointXccyCalibrationResult_Get_DomesticBlock_
{
    XlRegister_JointXccyCalibrationResult_Get_DomesticBlock_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("The joint calibration result");
        Excel::Register("Base", "xl_JointXccyCalibrationResult_Get_DomesticBlock", "JOINTXCCYCALIBRATIONRESULT.GET.DOMESTICBLOCK", "Extract the solved domestic curve block from a joint XCCY calibration result", "QQ", "result", argHelp, false);
    }
};
static XlRegister_JointXccyCalibrationResult_Get_DomesticBlock_ The_JointXccyCalibrationResult_Get_DomesticBlock_XlRegisterer;
