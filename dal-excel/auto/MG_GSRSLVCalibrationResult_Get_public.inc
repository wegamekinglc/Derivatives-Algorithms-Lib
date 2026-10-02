

extern "C" __declspec(dllexport) OPER_* xl_GSRSLVCalibrationResult_Get
    (const OPER_* xl_result, const OPER_* xl_attribute)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("GSRSLVCalibrationResult_Get");        
        argName = "result (input #1)";
        const Handle_<StorableGSRSLVCalibrationResult_> result = Excel::ToHandle<StorableGSRSLVCalibrationResult_>(_env, xl_result);
        argName = "attribute (input #2)";
        const String_ attribute = Excel::ToString(xl_attribute);
        argName = 0;
		Matrix_<Cell_> value;
        GSRSLVCalibrationResult_Get(result, attribute, &value);
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

struct XlRegister_GSRSLVCalibrationResult_Get_
{
    XlRegister_GSRSLVCalibrationResult_Get_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Calibration result");
        argHelp.push_back("Diagnostic name");
        Excel::Register("Base", "xl_GSRSLVCalibrationResult_Get", "GSRSLVCALIBRATIONRESULT.GET", "Read an SLV calibration diagnostic", "QQQ", "result,attribute", argHelp, false);
    }
};
static XlRegister_GSRSLVCalibrationResult_Get_ The_GSRSLVCalibrationResult_Get_XlRegisterer;
