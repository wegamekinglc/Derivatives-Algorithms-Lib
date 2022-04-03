


namespace TALibNet
{
    public ref struct PasteWithArgs_Return_
    {
        System::String^ error_;        
        System::String^ func_with_args_;
    };

    public ref class PasteWithArgs_
    {
    public:
        static initonly PasteWithArgs_^ Instance = gcnew PasteWithArgs_;
        PasteWithArgs_Return_^ Run(System::String^ dn_func_name);
    };
}


