#pragma once

#include "rapidjson-macros/shared/macros.hpp"

DECLARE_JSON_CLASS(PlaylistCore, Difficulty,
    NAMED_AUTO_VALUE(std::string, Characteristic, "characteristic");
    NAMED_AUTO_VALUE(std::string, Name, "name");
)

DECLARE_JSON_CLASS(PlaylistCore, BPSong,
    NAMED_AUTO_VALUE(std::string, Hash, "hash");
    NAMED_AUTO_VALUE_OPTIONAL(std::string, SongName, "songName");
    NAMED_AUTO_VALUE_OPTIONAL(std::string, Key, "key");
    NAMED_AUTO_VECTOR_OPTIONAL(PlaylistCore::Difficulty, Difficulties, "difficulties");
)

DECLARE_JSON_CLASS(PlaylistCore, CustomData,
    NAMED_AUTO_VALUE_OPTIONAL(std::string, SyncURL, "syncURL");
)

DECLARE_JSON_CLASS(PlaylistCore, BPList,
    NAMED_AUTO_VALUE(std::string, PlaylistTitle, "playlistTitle");
    NAMED_AUTO_VALUE_OPTIONAL(std::string, PlaylistAuthor, "playlistAuthor");
    NAMED_AUTO_VALUE_OPTIONAL(std::string, PlaylistDescription, "playlistDescription");
    NAMED_AUTO_VECTOR(PlaylistCore::BPSong, Songs, "songs");
    NAMED_AUTO_VALUE_OPTIONAL(std::string, ImageString, "imageString");
    NAMED_AUTO_VALUE_OPTIONAL(PlaylistCore::CustomData, CustomData, "customData");
    DESERIALIZE_ACTION(0,
        if(jsonValue.HasMember("downloadURL") && jsonValue["downloadURL"].IsString()) {
            if(!outerClass->CustomData.has_value())
                outerClass->CustomData.emplace();
            outerClass->CustomData->SyncURL = jsonValue["downloadURL"].GetString();
        }
        if(jsonValue.HasMember("image") && jsonValue["image"].IsString()) {
            outerClass->ImageString = jsonValue["image"].GetString();
        }
    )
)