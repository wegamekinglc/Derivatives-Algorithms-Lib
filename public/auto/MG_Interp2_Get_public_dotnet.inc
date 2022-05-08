

#pragma managed
#include "MG_Interp2_Get_public_dotnet.hpp"

TALibNet::Interp2_Get_Return_^ TALibNet::Interp2_Get_::Run
    (TALibNet::Object_^ dn_f, array<System::Double >^ dn_x, array<System::Double >^ dn_y)
{
    Interp2_Get_Return_^ retval = gcnew Interp2_Get_Return_;
    const char* argName = 0;
    try
    {        
        argName = "f";
        const Handle_<Interp2_> f = DotNet::ToHandle(dn_f);
        argName = "x";
        const Vector_<double> x = DotNet::ToDoubleVector(dn_x);
        argName = "y";
        const Vector_<double> y = DotNet::ToDoubleVector(dn_y);
        argName = 0;
		Matrix_<double> z;
        Interp2_Get(f, x, y, &z);
        retval->z_ = DotNet::CopyOut(z);
        
        return retval;
    }
    catch (std::exception& e)
    {
        retval->error_ = DotNet::Error(e.what(), argName);
        return retval;
    }
}

