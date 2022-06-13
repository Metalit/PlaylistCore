#pragma once

#include "rapidjson-macros/shared/macros.hpp"

DECLARE_JSON_CLASS(PlaylistCore, PlaylistConfig,
    NAMED_AUTO_VALUE_DEFAULT(float, ScrollSpeed, 2, "scrollSpeed")
    NAMED_AUTO_VECTOR(std::string, Order, "order")
)

// defined in Main.cpp
extern PlaylistCore::PlaylistConfig playlistConfig;

void SaveConfig();

void UpdateScrollSpeed();
