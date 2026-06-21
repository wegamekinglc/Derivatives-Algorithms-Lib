

extern "C" __declspec(dllexport) OPER_* xl_CalibrationResult_Get
    (const OPER_* xl_result, const OPER_* xl_attribute)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("CalibrationResult_Get");        
        argName = "result (input #1)";
        const Handle_<StorableCurveCalibrationResult_> result = Excel::ToHandle<StorableCurveCalibrationResult_>(_env, xl_result);
        argName = "attribute (input #2)";
        const String_ attribute = Excel::ToString(xl_attribute);
        argName = 0;
		Matrix_<Cell_> value;
        CalibrationResult_Get(result, attribute, &value);
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

struct XlRegister_CalibrationResult_Get_
{
    XlRegister_CalibrationResult_Get_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("The calibration result handle (from CALIBRATE.SINGLECURVE)");
        argHelp.push_back("Attribute name: marketRates, modelRates, residuals, maxAbsResidual, rmsResidual");
        Excel::Register("Base", "xl_CalibrationResult_Get", "CALIBRATIONRESULT.GET", "Extract a diagnostic attribute from a curve calibration result. Use CALIBRATIONRESULT.GET.CURVE for the curve itself.", "QQQ", "result,attribute", argHelp, false);
    }
};
static XlRegister_CalibrationResult_Get_ The_CalibrationResult_Get_XlRegisterer;
