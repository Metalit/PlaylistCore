#pragma once

#include "reflectcpp/include/rfl.hpp"

namespace PlaylistCore {
    struct Difficulty {
        std::string characteristic;
        std::string name;
        rfl::ExtraFields<rfl::Generic> extraFields;
    };

    struct BPSong {
        std::string levelid;
        std::optional<std::string> hash;
        std::optional<std::string> songName;
        std::optional<std::string> key;
        std::optional<std::vector<Difficulty>> difficulties;
        rfl::ExtraFields<rfl::Generic> extraFields;
    };

    struct CustomData {
        std::optional<std::string> syncURL;
        rfl::ExtraFields<rfl::Generic> extraFields;
    };

    struct BPList {
        std::string playlistTitle;
        std::optional<std::string> playlistAuthor;
        std::optional<std::string> playlistDescription;
        std::vector<BPSong> songs;
        std::optional<std::string> image;
        std::optional<CustomData> customData;
        rfl::ExtraFields<rfl::Generic> extraFields;
    };

    std::optional<BPList> ReadPlaylistJSON(std::string_view json);
    std::string WritePlaylistJSON(BPList const& playlist);
}
