

extern "C" __declspec(dllexport) OPER_* xl_MonteCarlo_Value
    (const OPER_* xl_product, const OPER_* xl_modelData, const OPER_* xl_n_paths, const OPER_* xl_rsg, const OPER_* xl_use_bb)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("MonteCarlo_Value");        
        argName = "product (input #1)";
        const Handle_<ScriptProduct_> product = Excel::ToHandle<ScriptProduct_>(_env, xl_product);
        argName = "modelData (input #2)";
        const Handle_<ModelData_> modelData = Excel::ToHandle<ModelData_>(_env, xl_modelData);
        argName = "n_paths (input #3)";
        const double n_paths = Excel::ToDouble(xl_n_paths);
        argName = "rsg (input #4)";
        const String_ rsg = Excel::ToString(xl_rsg);
        argName = "use_bb (input #5)";
        const bool use_bb = Excel::ToBool(xl_use_bb);
        argName = 0;
		Matrix_<Cell_> values;
        MonteCarlo_Value(product, modelData, n_paths, rsg, use_bb, &values);
        Excel::Retval_ retval;
        retval.Load(values);
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

struct XlRegister_MonteCarlo_Value_
{
    XlRegister_MonteCarlo_Value_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("a product");
        argHelp.push_back("a model's data");
        argHelp.push_back("# of paths need for valuation");
        argHelp.push_back("method of random number generation");
        argHelp.push_back("whether to use brownian bridge to generate path");
        Excel::Register("Base", "xl_MonteCarlo_Value", "MONTECARLO.VALUE", "valuation with monte carlo by a script product and a dedicated model", "QQQQQQ", "product,modelData,n_paths,rsg,use_bb", argHelp, false);
    }
};
static XlRegister_MonteCarlo_Value_ The_MonteCarlo_Value_XlRegisterer;
