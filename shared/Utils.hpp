#pragma once

#include "UnityEngine/Texture2D.hpp"
#include "GlobalNamespace/IPreviewBeatmapLevel.hpp"
#include "GlobalNamespace/IBeatmapLevelPack.hpp"

namespace PlaylistCore {
    class Playlist;

    namespace Utils {

        struct SelectionState {
            PlaylistCore::Playlist* selectedPlaylist;
            int selectedPlaylistIdx;
            GlobalNamespace::IPreviewBeatmapLevel* selectedSong;
            int selectedSongIdx;
        };

        std::string GetLevelHash(GlobalNamespace::IPreviewBeatmapLevel* level);

        bool IsWipLevel(GlobalNamespace::IPreviewBeatmapLevel* level);

        void RemoveAllBMBFSuffixes();

        std::string SanitizeFileName(std::string_view fileName);

        bool UniqueFileName(std::string_view fileName, std::string_view compareDirectory);

        std::string GetNewPlaylistPath(std::string_view title);

        std::string GetPlaylistBackupPath(std::string_view path);

        std::string GetBase64ImageType(std::string_view base64);

        std::string ProcessImage(UnityEngine::Texture2D* texture, bool returnPngString);

        void WriteImageToFile(std::string_view pathToPng, UnityEngine::Texture2D* texture);

        List<GlobalNamespace::IBeatmapLevelPack*>* GetCustomPacks();

        void SetCustomPacks(List<GlobalNamespace::IBeatmapLevelPack*>* newPlaylists, bool updateSongs);

        SelectionState GetSelectionState();

        void SetSelectionState(const SelectionState& state);

        void ReloadSongsKeepingSelection(std::function<void()> finishCallback = nullptr);

        void ReloadPlaylistsKeepingSelection();
    }
}
