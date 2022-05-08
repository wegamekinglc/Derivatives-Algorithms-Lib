

#pragma managed
#include "MG_Interp1_New_Cubic_public_dotnet.hpp"

TALibNet::Interp1_New_Cubic_Return_^ TALibNet::Interp1_New_Cubic_::Run
    (System::String^ dn_name, array<System::Double >^ dn_x, array<System::Double >^ dn_y, array<System::Int32 >^ dn_boundary_order, array<System::Double >^ dn_boundary_value)
{
    Interp1_New_Cubic_Return_^ retval = gcnew Interp1_New_Cubic_Return_;
    const char* argName = 0;
    try
    {        
        argName = "name";
        const String_ name = DotNet::ToString(dn_name);
        argName = "x";
        const Vector_<double> x = DotNet::ToDoubleVector(dn_x);
        argName = "y";
        const Vector_<double> y = DotNet::ToDoubleVector(dn_y);
        argName = "boundary_order";
        const Vector_<int> boundary_order = DotNet::ToIntVector(dn_boundary_order, true);
        argName = "boundary_value";
        const Vector_<double> boundary_value = DotNet::ToDoubleVector(dn_boundary_value, true);
        argName = 0;
		Handle_<Interp1_> f;
        Interp1_New_Cubic(name, x, y, boundary_order, boundary_value, &f);
        retval->f_ = DotNet::CopyOut(f);
        
        return retval;
    }
    catch (std::exception& e)
    {
        retval->error_ = DotNet::Error(e.what(), argName);
        return retval;
    }
}

