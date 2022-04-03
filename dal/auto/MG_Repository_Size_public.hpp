


namespace TALibNet
{
    public ref struct Repository_Size_Return_
    {
        System::String^ error_;        
        System::Int32 size_;
    };

    public ref class Repository_Size_
    {
    public:
        static initonly Repository_Size_^ Instance = gcnew Repository_Size_;
        Repository_Size_Return_^ Run();
    };
}


