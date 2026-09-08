#include "Types/BPList.hpp"

#include "Main.hpp"
#include "reflectcpp/include/rfl/json.hpp"

namespace rfl {
    template <>
    struct Reflector<PlaylistCore::BPSong> {
        struct ReflType {
            std::optional<std::string> levelid;
            std::optional<std::string> hash;
            std::optional<std::string> songName;
            std::optional<std::string> key;
            std::optional<std::vector<PlaylistCore::Difficulty>> difficulties;
            rfl::ExtraFields<rfl::Generic> extraFields;
        };

        static PlaylistCore::BPSong to(ReflType const& v) {
            if (!v.hash && !v.levelid)
                throw std::runtime_error("levelid or hash is required");
            auto levelid = v.levelid ? *v.levelid : CustomLevelPackPrefixID + *v.hash;
            return {levelid, v.hash, v.songName, v.key, v.difficulties, v.extraFields};
        }

        static ReflType from(PlaylistCore::BPSong const& v) { return {v.levelid, v.hash, v.songName, v.key, v.difficulties, v.extraFields}; }
    };

    template <>
    struct Reflector<PlaylistCore::BPList> {
        struct ReflType {
            std::string playlistTitle;
            std::optional<std::string> playlistAuthor;
            std::optional<std::string> playlistDescription;
            std::vector<PlaylistCore::BPSong> songs;
            std::optional<std::string> imageString;
            std::optional<std::string> image;
            std::optional<PlaylistCore::CustomData> customData;
            rfl::ExtraFields<rfl::Generic> extraFields;
        };

        static PlaylistCore::BPList to(ReflType const& v) {
            auto image = v.imageString ? v.imageString : v.image;
            auto customData = v.customData;
            if (auto url = v.extraFields.get("downloadURL").and_then([](auto f) { return f.to_string(); })) {
                if (!customData)
                    customData.emplace();
                customData->syncURL = url.value();
            }
            return {v.playlistTitle, v.playlistAuthor, v.playlistDescription, v.songs, image, customData, v.extraFields};
        }

        static ReflType from(PlaylistCore::BPList const& v) {
            return {v.playlistTitle, v.playlistAuthor, v.playlistDescription, v.songs, v.image, std::nullopt, v.customData, v.extraFields};
        }
    };
}

std::optional<PlaylistCore::BPList> PlaylistCore::ReadPlaylistJSON(std::string_view json) {
    auto result = rfl::json::read<BPList>(json);
    if (result)
        return *result;
    LOG_ERROR("Error loading playlist: {}", result.error().what());
    return std::nullopt;
}

std::string PlaylistCore::WritePlaylistJSON(PlaylistCore::BPList const& playlist) {
    return rfl::json::write(playlist);
}
