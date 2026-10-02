

extern "C" __declspec(dllexport) OPER_* xl_Calibrate_GSRSLVMarket
    (const OPER_* xl_initial, const OPER_* xl_quotes, const OPER_* xl_parameters, const OPER_* xl_settings, const OPER_* xl_heldOut)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("Calibrate_GSRSLVMarket");        
        argName = "initial (input #1)";
        const Handle_<ModelData_> initial = Excel::ToHandle<ModelData_>(_env, xl_initial);
        argName = "quotes (input #2)";
        const Vector_<Handle_<Storable_>> quotes = Excel::ToHandleBaseVector(_env, xl_quotes);
        argName = "parameters (input #3)";
        const Matrix_<Cell_> parameters = Excel::ToCellMatrix(xl_parameters);
        argName = "settings (input #4)";
        const Matrix_<Cell_> settings = Excel::ToCellMatrix(xl_settings, true);
        argName = "heldOut (input #5)";
        const Vector_<Handle_<Storable_>> heldOut = Excel::ToHandleBaseVector(_env, xl_heldOut, true);
        argName = 0;
		Handle_<StorableGSRSLVCalibrationResult_> result;
        Calibrate_GSRSLVMarket(initial, quotes, parameters, settings, heldOut, &result);
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

struct XlRegister_Calibrate_GSRSLVMarket_
{
    XlRegister_Calibrate_GSRSLVMarket_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Initial GSRSLVModelData");
        argHelp.push_back("Market quote handles");
        argHelp.push_back("Label, lower, upper, scale");
        argHelp.push_back("Solver and Monte Carlo settings");
        argHelp.push_back("Held-out market quote handles");
        Excel::Register("Base", "xl_Calibrate_GSRSLVMarket", "CALIBRATE.GSRSLVMARKET", "Calibrate SLV to market volatility quotes", "QQQQQQ", "initial,quotes,parameters,[settings],[heldOut]", argHelp, false);
    }
};
static XlRegister_Calibrate_GSRSLVMarket_ The_Calibrate_GSRSLVMarket_XlRegisterer;
