#pragma once

#include <dal/script/simulation.hpp>

Dal::Script::SimResults_ RunProductionStructure(const Dal::Script::PreparedScript_& product, const Dal::Handle_<Dal::ModelData_>& data, size_t paths);
