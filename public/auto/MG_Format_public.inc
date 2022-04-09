

extern "C" __declspec(dllexport) OPER_* xl_Format
    (const OPER_* xl_format, const OPER_* xl_arg1, const OPER_* xl_arg2, const OPER_* xl_arg3, const OPER_* xl_arg4, const OPER_* xl_arg5, const OPER_* xl_arg6, const OPER_* xl_arg7, const OPER_* xl_arg8, const OPER_* xl_arg9)	
{
    Excel::InitializeSessionIfNeeded();
	ENV_SEED_TYPE(ObjectAccess_);
    const char* argName = 0;
    try
    {	
        Logging::Write("Format");        
        argName = "format (input #1)";
        const String_ format = Excel::ToString(xl_format);
        argName = "arg1 (input #2)";
        const Matrix_<Cell_> arg1 = Excel::ToCellMatrix(xl_arg1);
        argName = "arg2 (input #3)";
        const Matrix_<Cell_> arg2 = Excel::ToCellMatrix(xl_arg2, true);
        argName = "arg3 (input #4)";
        const Matrix_<Cell_> arg3 = Excel::ToCellMatrix(xl_arg3, true);
        argName = "arg4 (input #5)";
        const Matrix_<Cell_> arg4 = Excel::ToCellMatrix(xl_arg4, true);
        argName = "arg5 (input #6)";
        const Matrix_<Cell_> arg5 = Excel::ToCellMatrix(xl_arg5, true);
        argName = "arg6 (input #7)";
        const Matrix_<Cell_> arg6 = Excel::ToCellMatrix(xl_arg6, true);
        argName = "arg7 (input #8)";
        const Matrix_<Cell_> arg7 = Excel::ToCellMatrix(xl_arg7, true);
        argName = "arg8 (input #9)";
        const Matrix_<Cell_> arg8 = Excel::ToCellMatrix(xl_arg8, true);
        argName = "arg9 (input #10)";
        const Matrix_<Cell_> arg9 = Excel::ToCellMatrix(xl_arg9, true);
        argName = 0;
		Matrix_<Cell_> result;
        Format(format, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, &result);
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

struct XlRegister_Format_
{
    XlRegister_Format_()
    {
        Vector_<String_> argHelp;        
        argHelp.push_back("The rule for combining arguments : e.g. 5, 1, (3, 4)T means join(from left to right) argument 5, then argument 1, then the transpose of the result of joining(from left to right) arguments 3 and 4.  The special argument 0 represents a single empty cell.");
        argHelp.push_back("A table");
        argHelp.push_back("A table");
        argHelp.push_back("A table");
        argHelp.push_back("A table");
        argHelp.push_back("A table");
        argHelp.push_back("A table");
        argHelp.push_back("A table");
        argHelp.push_back("A table");
        argHelp.push_back("A table");
        Excel::Register("Bones", "xl_Format", "DA.FORMAT", "Combine data according to a user - supplied format string", "QQQQQQQQQQQ", "format,arg1,[arg2],[arg3],[arg4],[arg5],[arg6],[arg7],[arg8],[arg9]", argHelp, false);
    }
};
static XlRegister_Format_ The_Format_XlRegisterer;

