

#pragma managed
#include "MG_Format_public_dotnet.hpp"

TALibNet::Format_Return_^ TALibNet::Format_::Run
    (System::String^ dn_format, array<array<TALibNet::Atom_^ >^ >^ dn_arg1, array<array<TALibNet::Atom_^ >^ >^ dn_arg2, array<array<TALibNet::Atom_^ >^ >^ dn_arg3, array<array<TALibNet::Atom_^ >^ >^ dn_arg4, array<array<TALibNet::Atom_^ >^ >^ dn_arg5, array<array<TALibNet::Atom_^ >^ >^ dn_arg6, array<array<TALibNet::Atom_^ >^ >^ dn_arg7, array<array<TALibNet::Atom_^ >^ >^ dn_arg8, array<array<TALibNet::Atom_^ >^ >^ dn_arg9)
{
    Format_Return_^ retval = gcnew Format_Return_;
    const char* argName = 0;
    try
    {        
        argName = "format";
        const String_ format = DotNet::ToString(dn_format);
        argName = "arg1";
        const Matrix_<Cell_> arg1 = DotNet::ToCellMatrix(dn_arg1);
        argName = "arg2";
        const Matrix_<Cell_> arg2 = DotNet::ToCellMatrix(dn_arg2, true);
        argName = "arg3";
        const Matrix_<Cell_> arg3 = DotNet::ToCellMatrix(dn_arg3, true);
        argName = "arg4";
        const Matrix_<Cell_> arg4 = DotNet::ToCellMatrix(dn_arg4, true);
        argName = "arg5";
        const Matrix_<Cell_> arg5 = DotNet::ToCellMatrix(dn_arg5, true);
        argName = "arg6";
        const Matrix_<Cell_> arg6 = DotNet::ToCellMatrix(dn_arg6, true);
        argName = "arg7";
        const Matrix_<Cell_> arg7 = DotNet::ToCellMatrix(dn_arg7, true);
        argName = "arg8";
        const Matrix_<Cell_> arg8 = DotNet::ToCellMatrix(dn_arg8, true);
        argName = "arg9";
        const Matrix_<Cell_> arg9 = DotNet::ToCellMatrix(dn_arg9, true);
        argName = 0;
		Matrix_<Cell_> result;
        Format(format, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, &result);
        retval->result_ = DotNet::CopyOut(result);
        
        return retval;
    }
    catch (std::exception& e)
    {
        retval->error_ = DotNet::Error(e.what(), argName);
        return retval;
    }
}

