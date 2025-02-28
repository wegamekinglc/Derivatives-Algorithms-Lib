

extern "C" __declspec(dllexport) OPER_* xl_DupireModelData_New
    (const OPER_* xl_name, const OPER_* xl_spot, const OPER_* xl_rate, const OPER_* xl_repo, const OPER_* xl_spots, const OPER_* xl_times, const OPER_* xl_vols)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("DupireModelData_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "spot (input #2)";
        const double spot = Excel::ToDouble(xl_spot);
        argName = "rate (input #3)";
        const double rate = Excel::ToDouble(xl_rate);
        argName = "repo (input #4)";
        const double repo = Excel::ToDouble(xl_repo);
        argName = "spots (input #5)";
        const Vector_<double> spots = Excel::ToDoubleVector(xl_spots);
        argName = "times (input #6)";
        const Vector_<double> times = Excel::ToDoubleVector(xl_times);
        argName = "vols (input #7)";
        const Matrix_<double> vols = Excel::ToDoubleMatrix(xl_vols);
        argName = 0;
		Handle_<ModelData_> model;
        DupireModelData_New(name, spot, rate, repo, spots, times, vols, &model);
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

struct XlRegister_DupireModelData_New_
{
    XlRegister_DupireModelData_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("A name for the object being created");
        argHelp.push_back("current spot value");
        argHelp.push_back("risk-free rate");
        argHelp.push_back("repo rate including dividend");
        argHelp.push_back("local vol surface spots data");
        argHelp.push_back("local vol surface times data");
        argHelp.push_back("local vol surface data");
        Excel::Register("Base", "xl_DupireModelData_New", "DUPIREMODELDATA.NEW", "Dupire local volality model's data description", "QQQQQQQQ", "name,spot,rate,repo,spots,times,vols", argHelp, false);
    }
};
static XlRegister_DupireModelData_New_ The_DupireModelData_New_XlRegisterer;
