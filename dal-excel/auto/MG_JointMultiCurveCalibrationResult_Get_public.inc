

extern "C" __declspec(dllexport) OPER_* xl_JointMultiCurveCalibrationResult_Get
    (const OPER_* xl_result, const OPER_* xl_attribute)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("JointMultiCurveCalibrationResult_Get");        
        argName = "result (input #1)";
        const Handle_<StorableJointMultiCurveCalibrationResult_> result = Excel::ToHandle<StorableJointMultiCurveCalibrationResult_>(_env, xl_result);
        argName = "attribute (input #2)";
        const String_ attribute = Excel::ToString(xl_attribute);
        argName = 0;
		Matrix_<Cell_> value;
        JointMultiCurveCalibrationResult_Get(result, attribute, &value);
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

struct XlRegister_JointMultiCurveCalibrationResult_Get_
{
    XlRegister_JointMultiCurveCalibrationResult_Get_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Generic joint calibration result");
        argHelp.push_back("Including jacobian, effJacobianInverse, parameterRanges, residualRanges, residualInstrumentOrdinals, marketRates, modelRates, residuals. See the result-field reference for scalar and inverse metadata selectors.");
        Excel::Register("Base", "xl_JointMultiCurveCalibrationResult_Get", "JOINTMULTICURVECALIBRATIONRESULT.GET", "Read a generic joint calibration matrix or diagnostic", "QQQ", "result,attribute", argHelp, false);
    }
};
static XlRegister_JointMultiCurveCalibrationResult_Get_ The_JointMultiCurveCalibrationResult_Get_XlRegisterer;
