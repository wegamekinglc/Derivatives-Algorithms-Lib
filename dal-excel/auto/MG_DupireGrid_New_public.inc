

extern "C" __declspec(dllexport) OPER_* xl_DupireGrid_New
    (const OPER_* xl_name, const OPER_* xl_inclusionSpots, const OPER_* xl_maxSpotSpacing, const OPER_* xl_inclusionTimes, const OPER_* xl_maxTimeSpacing)	
{
    Excel::InitializeSessionIfNeeded();
ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("DupireGrid_New");        
        argName = "name (input #1)";
        const String_ name = Excel::ToString(xl_name);
        argName = "name"; Excel::ValidateDupireTextInput(xl_name, "DupireGrid_New; name"); const Excel::ScriptSettingsInput_ inclusionSpotsInput(xl_inclusionSpots); xl_inclusionSpots = inclusionSpotsInput.Get();
        argName = "inclusionSpots (input #2)";
        const Vector_<double> inclusionSpots = Excel::ToDoubleVector(xl_inclusionSpots);
        const Excel::ScriptSettingsInput_ maxSpotSpacingInput(xl_maxSpotSpacing); xl_maxSpotSpacing = maxSpotSpacingInput.Get();
        argName = "maxSpotSpacing (input #3)";
        const double maxSpotSpacing = Excel::ToDouble(xl_maxSpotSpacing);
        const Excel::ScriptSettingsInput_ inclusionTimesInput(xl_inclusionTimes); xl_inclusionTimes = inclusionTimesInput.Get();
        argName = "inclusionTimes (input #4)";
        const Vector_<double> inclusionTimes = Excel::ToDoubleVector(xl_inclusionTimes);
        const Excel::ScriptSettingsInput_ maxTimeSpacingInput(xl_maxTimeSpacing); xl_maxTimeSpacing = maxTimeSpacingInput.Get();
        argName = "maxTimeSpacing (input #5)";
        const double maxTimeSpacing = Excel::ToDouble(xl_maxTimeSpacing);
        argName = 0;
		Handle_<StorableDupireGrid_> grid;
        DupireGrid_New(name, inclusionSpots, maxSpotSpacing, inclusionTimes, maxTimeSpacing, &grid);
        Excel::Retval_ retval;
        retval.Load(_env, grid);
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

struct XlRegister_DupireGrid_New_
{
    XlRegister_DupireGrid_New_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("Object name");
        argHelp.push_back("Positive increasing spot grid");
        argHelp.push_back("Maximum positive spot spacing");
        argHelp.push_back("Positive increasing time grid");
        argHelp.push_back("Maximum positive time spacing");
        Excel::Register("Base", "xl_DupireGrid_New", "DUPIREGRID.NEW", "Create immutable Dupire calibration grid settings", "QQQQQQ", "name,inclusionSpots,maxSpotSpacing,inclusionTimes,maxTimeSpacing", argHelp, false);
    }
};
static XlRegister_DupireGrid_New_ The_DupireGrid_New_XlRegisterer;
