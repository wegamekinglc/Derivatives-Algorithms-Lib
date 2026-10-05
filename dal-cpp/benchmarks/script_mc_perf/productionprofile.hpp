//
// Created by Codex on 2026/10/04.
//

#pragma once

#include <dal/platform/platform.hpp>

#include <dal/script/event.hpp>

using ExerciseProductFactory_ = Dal::Script::ScriptProductData_ (*)(const Dal::String_&, bool);

int RunProductionProfile(int argc, char** argv, ExerciseProductFactory_ exerciseFactory);
