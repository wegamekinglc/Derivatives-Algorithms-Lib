#include "common.hpp"

int main(int argc, char* argv[]) {
    Dal::Examples::Autocallables::Case_ terms;
    terms.title_ = "Capital-protected autocall";
    terms.coupon_ = 0.04;
    terms.phoenix_ = true;
    terms.protected_ = true;
    return Dal::Examples::Autocallables::Run(argc, argv, terms);
}
