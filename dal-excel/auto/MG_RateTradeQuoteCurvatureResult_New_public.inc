

extern "C" __declspec(dllexport) OPER_* xl_RateTradeQuoteCurvatureResult_New
    (const OPER_* xl_name, const OPER_* xl_trades, const OPER_* xl_calibration, const OPER_* xl_bumps, const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("RateTradeQuoteCurvatureResult_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "name"; Excel::ValidateRiskRequestText(xl_name, "RateTradeQuoteCurvatureResult_New; name");
        argName = "trades (input #2)";
        const Vector_<Handle_<Storable_>> trades = Excel::ToHandleBaseVector(_env, xl_trades);
        argName = "calibration (input #3)";
        const Handle_<StorableRateCalibrationSnapshot_> calibration = Excel::ToHandle<StorableRateCalibrationSnapshot_>(_env, xl_calibration);
        argName = "bumps (input #4)";
        const Handle_<StorableBumpOverAADRequest_> bumps = Excel::ToHandle<StorableBumpOverAADRequest_>(_env, xl_bumps);
        argName = "settings (input #5)";
        const Handle_<StorableRateTradeQuoteCurvatureSettings_> settings = Excel::ToHandle<StorableRateTradeQuoteCurvatureSettings_>(_env, xl_settings, true);
        argName = 0;
		Handle_<StorableRateTradeQuoteCurvatureResult_> result;
        RateTradeQuoteCurvatureResult_New(name, trades, calibration, bumps, settings, &result);
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

struct XlRegister_RateTradeQuoteCurvatureResult_New_
{
    XlRegister_RateTradeQuoteCurvatureResult_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Object name");
        argHelp.push_back("Native rate trade rows; every row is validated including zero-weight rows");
        argHelp.push_back("Owning EXACT square native calibration snapshot");
        argHelp.push_back("Full raw directions and positive steps; native numeric and recording caps remain separate");
        argHelp.push_back("Copied weights and history; blank uses unit weights and native history admission");
        Excel::Register("Base", "xl_RateTradeQuoteCurvatureResult_New", "RATETRADEQUOTECURVATURERESULT.NEW", "Evaluate signed rate quote Hessian products through full native recalibration", "QQQQQQ", "name,trades,calibration,bumps,[settings]", argHelp, false);
    }
};
static XlRegister_RateTradeQuoteCurvatureResult_New_ The_RateTradeQuoteCurvatureResult_New_XlRegisterer;
