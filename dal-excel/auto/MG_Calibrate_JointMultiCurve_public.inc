

extern "C" __declspec(dllexport) OPER_* xl_Calibrate_JointMultiCurve
    (const OPER_* xl_spec, const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("Calibrate_JointMultiCurve");        
        argName = "spec (input #1)";
        const Handle_<StorableJointMultiCurveCalibrationSpec_> spec = Excel::ToHandle<StorableJointMultiCurveCalibrationSpec_>(_env, xl_spec);
        argName = "settings (input #2)";
        const Matrix_<Cell_> settings = Excel::ToCellMatrix(xl_settings, true);
      REQUIRE(settings.Cols() == 2 || settings.Empty(), "must have two columns (key, value)");
        argName = 0;
		Handle_<StorableJointMultiCurveCalibrationResult_> result;
        Calibrate_JointMultiCurve(spec, settings, &result);
        Excel::Retval_ retval;
        retval.Load(_env, result);
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

struct XlRegister_Calibrate_JointMultiCurve_
{
    XlRegister_Calibrate_JointMultiCurve_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Generic joint spec");
        argHelp.push_back("Keys: jacobianMode (ANALYTIC|BUMPED), computeJacobianAtSolution (true), computeEffJacobianInverse (false). EXACT inverse requests may select a different underdetermined solution; see the fixed initial-Jacobian chart contract.");
        Excel::Register("Base", "xl_Calibrate_JointMultiCurve", "CALIBRATE.JOINTMULTICURVE", "Calibrate all declared curves in one joint solve", "QQQ", "spec,[settings]", argHelp, false);
    }
};
static XlRegister_Calibrate_JointMultiCurve_ The_Calibrate_JointMultiCurve_XlRegisterer;
