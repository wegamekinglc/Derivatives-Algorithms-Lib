


namespace TALibNet
{
    public ref struct Interp2_Get_Return_
    {
        System::String^ error_;        
        array<array<System::Double >^ >^ z_;
    };

    public ref class Interp2_Get_
    {
    public:
        static initonly Interp2_Get_^ Instance = gcnew Interp2_Get_;
        Interp2_Get_Return_^ Run(TALibNet::Object_^ dn_f, array<System::Double >^ dn_x, array<System::Double >^ dn_y);
    };
}


