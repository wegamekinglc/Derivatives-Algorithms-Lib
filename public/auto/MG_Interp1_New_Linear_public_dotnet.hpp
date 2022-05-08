


namespace TALibNet
{
    public ref struct Interp1_New_Linear_Return_
    {
        System::String^ error_;        
        TALibNet::Object_^ f_;
    };

    public ref class Interp1_New_Linear_
    {
    public:
        static initonly Interp1_New_Linear_^ Instance = gcnew Interp1_New_Linear_;
        Interp1_New_Linear_Return_^ Run(System::String^ dn_name, array<System::Double >^ dn_x, array<System::Double >^ dn_y);
    };
}


