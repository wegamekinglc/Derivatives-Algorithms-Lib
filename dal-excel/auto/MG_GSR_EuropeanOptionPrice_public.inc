

extern "C" __declspec(dllexport) OPER_* xl_GSR_EuropeanOptionPrice
    (const OPER_* xl_model, const OPER_* xl_option, const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("GSR_EuropeanOptionPrice");        
        argName = "model (input #1)";
        const Handle_<ModelData_> model = Excel::ToHandle<ModelData_>(_env, xl_model);
        argName = "option (input #2)";
        const Handle_<StorableEuropeanRateOption_> option = Excel::ToHandle<StorableEuropeanRateOption_>(_env, xl_option);
        argName = "settings (input #3)";
        const Matrix_<Cell_> settings = Excel::ToCellMatrix(xl_settings, true);
        argName = 0;
		Matrix_<Cell_> result;
        GSR_EuropeanOptionPrice(model, option, settings, &result);
        Excel::Retval_ retval;
        retval.Load(result);
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

struct XlRegister_GSR_EuropeanOptionPrice_
{
    XlRegister_GSR_EuropeanOptionPrice_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("GSRModelData or MultiFactorGSRModelData");
        argHelp.push_back("Bond option, caplet or swaption");
        argHelp.push_back("Key/value rows: quadratureOrder (16), estimateError (true)");
        Excel::Register("Base", "xl_GSR_EuropeanOptionPrice", "GSR.EUROPEANOPTIONPRICE", "Price a European rate option in a Gaussian short rate model", "QQQQ", "model,option,[settings]", argHelp, false);
    }
};
static XlRegister_GSR_EuropeanOptionPrice_ The_GSR_EuropeanOptionPrice_XlRegisterer;
