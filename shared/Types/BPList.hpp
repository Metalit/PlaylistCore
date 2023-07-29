#pragma once

#include "rapidjson-macros/shared/macros.hpp"

namespace PlaylistCore {
    DECLARE_JSON_CLASS(Difficulty,
        NAMED_VALUE(std::string, Characteristic, "characteristic");
        NAMED_VALUE(std::string, Name, "name");
    )

    DECLARE_JSON_CLASS(BPSong,
        NAMED_VALUE(std::string, Hash, "hash");
        NAMED_VALUE_OPTIONAL(std::string, SongName, "songName");
        NAMED_VALUE_OPTIONAL(std::string, Key, "key");
        NAMED_VECTOR_OPTIONAL(PlaylistCore::Difficulty, Difficulties, "difficulties");
    )

    DECLARE_JSON_CLASS(CustomData,
        NAMED_VALUE_OPTIONAL(std::string, SyncURL, "syncURL");
    )

    DECLARE_JSON_CLASS(BPList,
        NAMED_VALUE(std::string, PlaylistTitle, "playlistTitle");
        NAMED_VALUE_OPTIONAL(std::string, PlaylistAuthor, "playlistAuthor");
        NAMED_VALUE_OPTIONAL(std::string, PlaylistDescription, "playlistDescription");
        NAMED_VECTOR(PlaylistCore::BPSong, Songs, "songs");
        NAMED_VALUE_OPTIONAL(std::string, ImageString, NAME_OPTS("imageString", "image"));
        NAMED_VALUE_OPTIONAL(PlaylistCore::CustomData, CustomData, "customData");
        DESERIALIZE_ACTION(0,
            if(jsonValue.HasMember("downloadURL") && jsonValue["downloadURL"].IsString()) {
                if(!self->CustomData.has_value())
                    self->CustomData.emplace();
                self->CustomData->SyncURL = jsonValue["downloadURL"].GetString();
            }
        )
    )
}
