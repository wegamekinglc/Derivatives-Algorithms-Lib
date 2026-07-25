

extern "C" __declspec(dllexport) OPER_* xl_JointXccyCalibrationResult_Get
    (const OPER_* xl_result, const OPER_* xl_attribute)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("JointXccyCalibrationResult_Get");        
        argName = "result (input #1)";
        const Handle_<StorableJointXccyCalibrationResult_> result = Excel::ToHandle<StorableJointXccyCalibrationResult_>(_env, xl_result);
        argName = "attribute (input #2)";
        const String_ attribute = Excel::ToString(xl_attribute);
        argName = 0;
		Matrix_<Cell_> value;
        JointXccyCalibrationResult_Get(result, attribute, &value);
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

struct XlRegister_JointXccyCalibrationResult_Get_
{
    XlRegister_JointXccyCalibrationResult_Get_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("The joint calibration result");
        argHelp.push_back("Attribute: fxForwards, marketRates, modelRates, residuals, jacobian, effJacobianInverse, parameterRanges, or residualRanges. Handle views domesticBlock, foreignBlock, and basisCurve use their dedicated getter functions.");
        Excel::Register("Base", "xl_JointXccyCalibrationResult_Get", "JOINTXCCYCALIBRATIONRESULT.GET", "Extract a matrix-valued view from a joint XCCY calibration result. Use the dedicated getter functions for curve handles.", "QQQ", "result,attribute", argHelp, false);
    }
};
static XlRegister_JointXccyCalibrationResult_Get_ The_JointXccyCalibrationResult_Get_XlRegisterer;
