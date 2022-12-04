#ifndef DAL_MODELS_I
#define DAL_MODELS_I

%{
#include <public/src/models.hpp>
%}

%include matrix.i

%template(ModelData_) Handle_<ModelData_>;

%inline %{
    Handle_<ModelData_> BSModelData_New(double spot,
                                        double vol,
                                        double rate,
                                        double div) {
        return NewBSModelData("BSModelData_", spot, vol, rate, div);
    }

    Handle_<ModelData_> DupireModelData_New(double spot,
                                            const std::vector<double>& spots,
                                            const std::vector<double>& times,
                                            const Matrix_<double>& vols) {

    Vector_<> new_spots;
    for (int i = 0; i < spots.size(); ++i)
        new_spots.push_back(spots[i]);

    Vector_<> new_times;
    for (int i = 0; i < times.size(); ++i)
        new_times.push_back(times[i]);

    return NewDupireModelData("DupireModelData_", spot, new_spots, new_times, vols);
}
%}

#endif