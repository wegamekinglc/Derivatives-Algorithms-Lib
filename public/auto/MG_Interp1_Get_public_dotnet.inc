

#pragma managed
#include "MG_Interp1_Get_public_dotnet.hpp"

TALibNet::Interp1_Get_Return_^ TALibNet::Interp1_Get_::Run
    (TALibNet::Object_^ dn_f, array<System::Double >^ dn_x)
{
    Interp1_Get_Return_^ retval = gcnew Interp1_Get_Return_;
    const char* argName = 0;
    try
    {        
        argName = "f";
        const Handle_<Interp1_> f = DotNet::ToHandle(dn_f);
        argName = "x";
        const Vector_<double> x = DotNet::ToDoubleVector(dn_x);
        argName = 0;
		Vector_<double> y;
        Interp1_Get(f, x, &y);
        retval->y_ = DotNet::CopyOut(y);
        
        return retval;
    }
    catch (std::exception& e)
    {
        retval->error_ = DotNet::Error(e.what(), argName);
        return retval;
    }
}

