// This file is auto-generated by machinist. Please don't modify it manually.
namespace Bag {
    struct Reader_ : Archive::Reader_ {
        String_ name_;
        Vector_<Handle_<Storable_>> contents_;
        Vector_<String_> keys_;
        Reader_(const Archive::View_& src, Archive::Built_& share) {
            using namespace Archive::Utils;
            NOTE("Reading Bag from store");
            assert(src.Type() == "Bag");
            GetOptional(src, "name", &name_, std::mem_fn(&Archive::View_::AsString));
            GetMultiple(src, "contents", &contents_, Archive::Builder_<>(share, "contents", ""));
            GetOptional(src, "keys", &keys_, std::mem_fn(&Archive::View_::AsStringVector));
        }
        Storable_* Build() const;	// handwritten
        Storable_* Build(const Archive::View_& src, Archive::Built_& share) const {
            return Reader_(src, share).Build();
        }

        // constructor-through-registry (safer than default constructor)
        Reader_(void (*register_func)(const String_&, const Archive::Reader_*)) {
            register_func("Bag", this);
        }
    };
    static Reader_ TheData(Archive::Register);
}
	