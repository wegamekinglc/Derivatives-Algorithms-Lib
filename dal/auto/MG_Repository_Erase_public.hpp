


namespace TALibNet
{
    public ref struct Repository_Erase_Return_
    {
        System::String^ error_;        
        System::Int32 num_erased_;
    };

    public ref class Repository_Erase_
    {
    public:
        static initonly Repository_Erase_^ Instance = gcnew Repository_Erase_;
        Repository_Erase_Return_^ Run(array<TALibNet::Object_^ >^ dn_objects);
    };
}


