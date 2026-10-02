

extern "C" __declspec(dllexport) OPER_* xl_GSRCalibrationResult_Get
    (const OPER_* xl_result, const OPER_* xl_attribute)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("GSRCalibrationResult_Get");        
        argName = "result (input #1)";
        const Handle_<StorableGSRCalibrationResult_> result = Excel::ToHandle<StorableGSRCalibrationResult_>(_env, xl_result);
        argName = "attribute (input #2)";
        const String_ attribute = Excel::ToString(xl_attribute);
        argName = 0;
		Matrix_<Cell_> value;
        GSRCalibrationResult_Get(result, attribute, &value);
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

struct XlRegister_GSRCalibrationResult_Get_
{
    XlRegister_GSRCalibrationResult_Get_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Calibration result");
        argHelp.push_back("modelPrices, residuals, numericalErrors, parameters, activeBounds, quoteJacobian, converged, fitWithinTolerance, numericalValidationPassed, iterations, evaluations, objective, jacobianRank, jacobianConditionEstimate, terminationReason");
        Excel::Register("Base", "xl_GSRCalibrationResult_Get", "GSRCALIBRATIONRESULT.GET", "Read a Gaussian calibration diagnostic", "QQQ", "result,attribute", argHelp, false);
    }
};
static XlRegister_GSRCalibrationResult_Get_ The_GSRCalibrationResult_Get_XlRegisterer;
