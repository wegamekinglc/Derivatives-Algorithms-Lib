


namespace TALibNet
{
    public ref struct Repository_Find_Return_
    {
        System::String^ error_;        
        array<TALibNet::Object_^ >^ objects_;
    };

    public ref class Repository_Find_
    {
    public:
        static initonly Repository_Find_^ Instance = gcnew Repository_Find_;
        Repository_Find_Return_^ Run(System::String^ dn_match);
    };
}


