#include "common.hpp"

int main(int argc, char* argv[]) {
    Dal::Examples::Autocallables::Case_ terms;
    terms.title_ = "Airbag snowball";
    terms.airbag_ = true;
    return Dal::Examples::Autocallables::Run(argc, argv, terms);
}
