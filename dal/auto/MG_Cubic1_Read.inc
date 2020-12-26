// This file is auto-generated by machinist. Please don't modified it manually.
namespace Cubic1 {
    struct Reader_ : Archive::Reader_ {
        String_ name_;
        Vector_<double> x_;
        Vector_<double> f_;
        Vector_<double> fpp_;
        Reader_(const Archive::View_& src, Archive::Built_& share) {
            using namespace Archive::Utils;
            NOTE("Reading Cubic1 from store");
            assert(src.Type() == "Cubic1");
            GetOptional(src, "name", &name_, std::mem_fn(&Archive::View_::AsString));
            Get(src, "x", &x_, std::mem_fn(&Archive::View_::AsDoubleVector));
            Get(src, "f", &f_, std::mem_fn(&Archive::View_::AsDoubleVector));
            Get(src, "fpp", &fpp_, std::mem_fn(&Archive::View_::AsDoubleVector));
        }
        Cubic1_* Build() const
        {
         return new Cubic1_(name_, x_, f_, fpp_);
        }
        Cubic1_* Build(const Archive::View_& src, Archive::Built_& share) const {
            return Reader_(src, share).Build();
        }

        // constructor-through-registry (safer than default constructor)
        Reader_(void (*register_func)(const String_&, const Archive::Reader_*)) {
            register_func("Cubic1", this);
        }
    };
    static Reader_ TheData(Archive::Register);
}
	