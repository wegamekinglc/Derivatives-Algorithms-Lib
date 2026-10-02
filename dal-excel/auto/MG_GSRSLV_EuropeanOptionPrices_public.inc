

extern "C" __declspec(dllexport) OPER_* xl_GSRSLV_EuropeanOptionPrices
    (const OPER_* xl_model, const OPER_* xl_options, const OPER_* xl_settings)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("GSRSLV_EuropeanOptionPrices");        
        argName = "model (input #1)";
        const Handle_<ModelData_> model = Excel::ToHandle<ModelData_>(_env, xl_model);
        argName = "options (input #2)";
        const Vector_<Handle_<Storable_>> options = Excel::ToHandleBaseVector(_env, xl_options);
        argName = "settings (input #3)";
        const Matrix_<Cell_> settings = Excel::ToCellMatrix(xl_settings, true);
        argName = 0;
		Matrix_<Cell_> result;
        GSRSLV_EuropeanOptionPrices(model, options, settings, &result);
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

struct XlRegister_GSRSLV_EuropeanOptionPrices_
{
    XlRegister_GSRSLV_EuropeanOptionPrices_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("GSRSLVModelData");
        argHelp.push_back("European option handles");
        argHelp.push_back("Key/value rows: paths (4096), seed (1729)");
        Excel::Register("Base", "xl_GSRSLV_EuropeanOptionPrices", "GSRSLV.EUROPEANOPTIONPRICES", "Price SLV European options on common antithetic paths", "QQQQ", "model,options,[settings]", argHelp, false);
    }
};
static XlRegister_GSRSLV_EuropeanOptionPrices_ The_GSRSLV_EuropeanOptionPrices_XlRegisterer;
