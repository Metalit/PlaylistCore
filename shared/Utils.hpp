#pragma once

#include "UnityEngine/Texture2D.hpp"
#include "GlobalNamespace/BeatmapLevel.hpp"
#include "GlobalNamespace/BeatmapLevelPack.hpp"

namespace PlaylistCore {
    class Playlist;

    namespace Utils {

        struct SelectionState {
            PlaylistCore::Playlist* selectedPlaylist;
            int selectedPlaylistIdx;
            GlobalNamespace::BeatmapLevel* selectedSong;
            int selectedSongIdx;
        };

        std::string GetLevelHash(GlobalNamespace::BeatmapLevel* level);

        bool IsWipLevel(GlobalNamespace::BeatmapLevel* level);

        void RemoveAllBMBFSuffixes();

        std::string SanitizeFileName(std::string_view fileName);

        bool UniqueFileName(std::string_view fileName, std::string_view compareDirectory);

        std::string GetNewPlaylistPath(std::string_view title);

        std::string GetPlaylistBackupPath(std::string_view path);

        std::string GetBase64ImageType(std::string_view base64);

        std::string ProcessImage(UnityEngine::Texture2D* texture, bool returnPngString);

        void WriteImageToFile(std::string_view pathToPng, UnityEngine::Texture2D* texture);

        List<GlobalNamespace::BeatmapLevelPack*>* GetCustomPacks();

        void SetCustomPacks(List<GlobalNamespace::BeatmapLevelPack*>* newPlaylists, bool updateSongs);

        SelectionState GetSelectionState();

        void SetSelectionState(const SelectionState& state);

        void ReloadSongsKeepingSelection(std::function<void()> finishCallback = nullptr);

        void ReloadPlaylistsKeepingSelection();
    }
}
