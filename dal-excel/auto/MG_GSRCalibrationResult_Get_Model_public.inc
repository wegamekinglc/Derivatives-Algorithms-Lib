

extern "C" __declspec(dllexport) OPER_* xl_GSRCalibrationResult_Get_Model
    (const OPER_* xl_result)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("GSRCalibrationResult_Get_Model");        
        argName = "result (input #1)";
        const Handle_<StorableGSRCalibrationResult_> result = Excel::ToHandle<StorableGSRCalibrationResult_>(_env, xl_result);
        argName = 0;
		Handle_<ModelData_> model;
        GSRCalibrationResult_Get_Model(result, &model);
        Excel::Retval_ retval;
        retval.Load(_env, model);
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

struct XlRegister_GSRCalibrationResult_Get_Model_
{
    XlRegister_GSRCalibrationResult_Get_Model_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Calibration result");
        Excel::Register("Base", "xl_GSRCalibrationResult_Get_Model", "GSRCALIBRATIONRESULT.GET.MODEL", "Extract the calibrated Gaussian model", "QQ", "result", argHelp, false);
    }
};
static XlRegister_GSRCalibrationResult_Get_Model_ The_GSRCalibrationResult_Get_Model_XlRegisterer;
