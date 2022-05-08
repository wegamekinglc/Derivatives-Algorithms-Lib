

#pragma managed
#include "MG_Interp1_New_Linear_Smoothed_public_dotnet.hpp"

TALibNet::Interp1_New_Linear_Smoothed_Return_^ TALibNet::Interp1_New_Linear_Smoothed_::Run
    (System::String^ dn_name, array<System::Double >^ dn_x, array<System::Double >^ dn_y, System::Double dn_smoothing, array<System::Double >^ dn_fit_weights)
{
    Interp1_New_Linear_Smoothed_Return_^ retval = gcnew Interp1_New_Linear_Smoothed_Return_;
    const char* argName = 0;
    try
    {        
        argName = "name";
        const String_ name = DotNet::ToString(dn_name);
        argName = "x";
        const Vector_<double> x = DotNet::ToDoubleVector(dn_x);
        argName = "y";
        const Vector_<double> y = DotNet::ToDoubleVector(dn_y);
        argName = "smoothing";
        const double smoothing = DotNet::ToDouble(dn_smoothing);
        argName = "fit_weights";
        const Vector_<double> fit_weights = DotNet::ToDoubleVector(dn_fit_weights, true);
        argName = 0;
		Handle_<Interp1_> f;
        Interp1_New_Linear_Smoothed(name, x, y, smoothing, fit_weights, &f);
        retval->f_ = DotNet::CopyOut(f);
        
        return retval;
    }
    catch (std::exception& e)
    {
        retval->error_ = DotNet::Error(e.what(), argName);
        return retval;
    }
}

