


namespace TALibNet
{
    public ref struct Interp1_Get_Return_
    {
        System::String^ error_;        
        array<System::Double >^ y_;
    };

    public ref class Interp1_Get_
    {
    public:
        static initonly Interp1_Get_^ Instance = gcnew Interp1_Get_;
        Interp1_Get_Return_^ Run(TALibNet::Object_^ dn_f, array<System::Double >^ dn_x);
    };
}


