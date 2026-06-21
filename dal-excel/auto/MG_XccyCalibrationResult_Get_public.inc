

extern "C" __declspec(dllexport) OPER_* xl_XccyCalibrationResult_Get
    (const OPER_* xl_result, const OPER_* xl_attribute)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("XccyCalibrationResult_Get");        
        argName = "result (input #1)";
        const Handle_<StorableCrossCurrencyCalibrationResult_> result = Excel::ToHandle<StorableCrossCurrencyCalibrationResult_>(_env, xl_result);
        argName = "attribute (input #2)";
        const String_ attribute = Excel::ToString(xl_attribute);
        argName = 0;
		Matrix_<Cell_> value;
        XccyCalibrationResult_Get(result, attribute, &value);
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

struct XlRegister_XccyCalibrationResult_Get_
{
    XlRegister_XccyCalibrationResult_Get_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("The cross-currency calibration result handle (from CALIBRATE.XCCYMARKET)");
        argHelp.push_back("Attribute name: marketRates, modelRates, residuals, maxAbsResidual, rmsResidual");
        Excel::Register("Base", "xl_XccyCalibrationResult_Get", "XCCYCALIBRATIONRESULT.GET", "Extract a diagnostic attribute from a cross-currency calibration result. Use XCCYCALIBRATIONRESULT.GET.BASISCURVE for the basis curve itself.", "QQQ", "result,attribute", argHelp, false);
    }
};
static XlRegister_XccyCalibrationResult_Get_ The_XccyCalibrationResult_Get_XlRegisterer;
