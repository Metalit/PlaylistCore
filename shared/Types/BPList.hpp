#pragma once

#include "rapidjson-macros/shared/macros.hpp"

namespace PlaylistCore {
    DECLARE_JSON_STRUCT(Difficulty) {
        NAMED_VALUE(std::string, Characteristic, "characteristic");
        NAMED_VALUE(std::string, Name, "name");
    };

    DECLARE_JSON_STRUCT(BPSong) {
        NAMED_VALUE_OPTIONAL(std::string, Hash, "hash");

       private:
        NAMED_VALUE_OPTIONAL(std::string, LevelID_Opt, "levelid");
        DESERIALIZE_FUNCTION(SetID) {
            if (!Hash && !LevelID_Opt)
                throw JSONException("Song had no ID and no hash");
            if (!LevelID_Opt)
                LevelID_Opt = "custom_level_" + *Hash;
            LevelID = *LevelID_Opt;
        }
        SERIALIZE_FUNCTION(SetID) {
            if (!LevelID_Opt)
                jsonObject.AddMember("levelid", LevelID, allocator);
            else
                jsonObject["levelid"].SetString(LevelID, allocator);
        }

       public:
        std::string LevelID;
        NAMED_VALUE_OPTIONAL(std::string, SongName, "songName");
        NAMED_VALUE_OPTIONAL(std::string, Key, "key");
        NAMED_VECTOR_OPTIONAL(PlaylistCore::Difficulty, Difficulties, "difficulties");
    };

    DECLARE_JSON_STRUCT(CustomData) {
        NAMED_VALUE_OPTIONAL(std::string, SyncURL, "syncURL");
    };

    DECLARE_JSON_STRUCT(BPList) {
        NAMED_VALUE(std::string, PlaylistTitle, "playlistTitle");
        NAMED_VALUE_OPTIONAL(std::string, PlaylistAuthor, "playlistAuthor");
        NAMED_VALUE_OPTIONAL(std::string, PlaylistDescription, "playlistDescription");
        NAMED_VECTOR(PlaylistCore::BPSong, Songs, "songs");
        NAMED_VALUE_OPTIONAL(std::string, ImageString, NAME_OPTS("imageString", "image"));
        NAMED_VALUE_OPTIONAL(PlaylistCore::CustomData, CustomData, "customData");
        DESERIALIZE_FUNCTION(SetSyncURL) {
            if (jsonValue.HasMember("downloadURL") && jsonValue["downloadURL"].IsString()) {
                if (!CustomData.has_value())
                    CustomData.emplace();
                CustomData->SyncURL = jsonValue["downloadURL"].GetString();
            }
        };
    };
}
