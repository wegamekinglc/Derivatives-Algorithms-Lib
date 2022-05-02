


namespace TALibNet
{
    public ref struct Interp2_New_Linear_Return_
    {
        System::String^ error_;        
        TALibNet::Object_^ f_;
    };

    public ref class Interp2_New_Linear_
    {
    public:
        static initonly Interp2_New_Linear_^ Instance = gcnew Interp2_New_Linear_;
        Interp2_New_Linear_Return_^ Run(System::String^ dn_name, array<System::Double >^ dn_x, array<System::Double >^ dn_y, array<array<System::Double >^ >^ dn_z);
    };
}


