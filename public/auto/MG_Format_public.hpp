


namespace TALibNet
{
    public ref struct Format_Return_
    {
        System::String^ error_;        
        array<array<TALibNet::Atom_^ >^ >^ result_;
    };

    public ref class Format_
    {
    public:
        static initonly Format_^ Instance = gcnew Format_;
        Format_Return_^ Run(System::String^ dn_format, array<array<TALibNet::Atom_^ >^ >^ dn_arg1, array<array<TALibNet::Atom_^ >^ >^ dn_arg2, array<array<TALibNet::Atom_^ >^ >^ dn_arg3, array<array<TALibNet::Atom_^ >^ >^ dn_arg4, array<array<TALibNet::Atom_^ >^ >^ dn_arg5, array<array<TALibNet::Atom_^ >^ >^ dn_arg6, array<array<TALibNet::Atom_^ >^ >^ dn_arg7, array<array<TALibNet::Atom_^ >^ >^ dn_arg8, array<array<TALibNet::Atom_^ >^ >^ dn_arg9);
    };
}


