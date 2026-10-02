

extern "C" __declspec(dllexport) OPER_* xl_GSRSLV_MarketQuoteRisk
    (const OPER_* xl_initial, const OPER_* xl_quotes, const OPER_* xl_parameters, const OPER_* xl_targets, const OPER_* xl_settings, const OPER_* xl_riskSettings, const OPER_* xl_curveRisk)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("GSRSLV_MarketQuoteRisk");        
        argName = "initial (input #1)";
        const Handle_<ModelData_> initial = Excel::ToHandle<ModelData_>(_env, xl_initial);
        argName = "quotes (input #2)";
        const Vector_<Handle_<Storable_>> quotes = Excel::ToHandleBaseVector(_env, xl_quotes);
        argName = "parameters (input #3)";
        const Matrix_<Cell_> parameters = Excel::ToCellMatrix(xl_parameters);
        argName = "targets (input #4)";
        const Vector_<Handle_<Storable_>> targets = Excel::ToHandleBaseVector(_env, xl_targets);
        argName = "settings (input #5)";
        const Matrix_<Cell_> settings = Excel::ToCellMatrix(xl_settings, true);
        argName = "riskSettings (input #6)";
        const Matrix_<Cell_> riskSettings = Excel::ToCellMatrix(xl_riskSettings, true);
        argName = "curveRisk (input #7)";
        const Handle_<StorableGSRCurveQuoteRisk_> curveRisk = Excel::ToHandle<StorableGSRCurveQuoteRisk_>(_env, xl_curveRisk, true);
        argName = 0;
		Handle_<StorableGSRSLVQuoteRiskResult_> result;
        GSRSLV_MarketQuoteRisk(initial, quotes, parameters, targets, settings, riskSettings, curveRisk, &result);
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

struct XlRegister_GSRSLV_MarketQuoteRisk_
{
    XlRegister_GSRSLV_MarketQuoteRisk_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Original GSRSLVModelData");
        argHelp.push_back("Market quote handles");
        argHelp.push_back("Label, lower, upper, scale");
        argHelp.push_back("European target option handles");
        argHelp.push_back("Calibration settings");
        argHelp.push_back("relativeBump, absoluteBump, stabilityTolerance");
        argHelp.push_back("Curve quote provenance bridge");
        Excel::Register("Base", "xl_GSRSLV_MarketQuoteRisk", "GSRSLV.MARKETQUOTERISK", "Recalibrate market volatility and curve quote bumps", "QQQQQQQQ", "initial,quotes,parameters,targets,[settings],[riskSettings],[curveRisk]", argHelp, false);
    }
};
static XlRegister_GSRSLV_MarketQuoteRisk_ The_GSRSLV_MarketQuoteRisk_XlRegisterer;
