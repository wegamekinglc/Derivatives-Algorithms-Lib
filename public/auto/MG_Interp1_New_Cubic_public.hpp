


namespace TALibNet
{
    public ref struct Interp1_New_Cubic_Return_
    {
        System::String^ error_;        
        TALibNet::Object_^ f_;
    };

    public ref class Interp1_New_Cubic_
    {
    public:
        static initonly Interp1_New_Cubic_^ Instance = gcnew Interp1_New_Cubic_;
        Interp1_New_Cubic_Return_^ Run(System::String^ dn_name, array<System::Double >^ dn_x, array<System::Double >^ dn_y, array<System::Int32 >^ dn_boundary_order, array<System::Double >^ dn_boundary_value);
    };
}


