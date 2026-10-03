

extern "C" __declspec(dllexport) OPER_* xl_Calibrate_GSRVolatility
    (const OPER_* xl_initial, const OPER_* xl_quotes, const OPER_* xl_parameters, const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("Calibrate_GSRVolatility");        
        argName = "initial (input #1)";
        const Handle_<ModelData_> initial = Excel::ToHandle<ModelData_>(_env, xl_initial);
        argName = "quotes (input #2)";
        const Vector_<Handle_<Storable_>> quotes = Excel::ToHandleBaseVector(_env, xl_quotes);
        argName = "parameters (input #3)";
        const Matrix_<double> parameters = Excel::ToDoubleMatrix(xl_parameters);
        argName = "settings (input #4)";
        const Matrix_<Cell_> settings = Excel::ToCellMatrix(xl_settings, true);
        argName = 0;
		Handle_<StorableGSRCalibrationResult_> result;
        Calibrate_GSRVolatility(initial, quotes, parameters, settings, &result);
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

struct XlRegister_Calibrate_GSRVolatility_
{
    XlRegister_Calibrate_GSRVolatility_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Initial MultiFactorGSRModelData");
        argHelp.push_back("CalibrationQuote handles in residual order");
        argHelp.push_back("Four columns: factor index, knot index, lower bound, upper bound");
        argHelp.push_back("Key/value rows: maxIterations, quadratureOrder, gradientTolerance, stepTolerance, finiteDifferenceStep, parameterScale, priorWeight, smoothingWeight, numericalErrorFraction");
        Excel::Register("Base", "xl_Calibrate_GSRVolatility", "CALIBRATE.GSRVOLATILITY", "Fit selected g buckets with curves, H and correlations fixed", "QQQQQ", "initial,quotes,parameters,[settings]", argHelp, false);
    }
};
static XlRegister_Calibrate_GSRVolatility_ The_Calibrate_GSRVolatility_XlRegisterer;
