#include "common.hpp"

int main(int argc, char* argv[]) {
    Dal::Examples::Autocallables::Case_ terms;
    terms.title_ = "Upside-participating autocall";
    terms.coupon_ = 0.06;
    terms.knockOut_ = 1.2;
    terms.participation_ = 1.0;
    return Dal::Examples::Autocallables::Run(argc, argv, terms);
}
