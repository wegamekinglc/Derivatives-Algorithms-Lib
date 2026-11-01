#include "common.hpp"

int main(int argc, char* argv[]) {
    Dal::Examples::Autocallables::Case_ terms;
    terms.title_ = "Phoenix conditional coupon";
    terms.phoenix_ = true;
    return Dal::Examples::Autocallables::Run(argc, argv, terms);
}
