#pragma once

#include "UnityEngine/Texture2D.hpp"
#include "GlobalNamespace/BeatmapLevel.hpp"
#include "GlobalNamespace/BeatmapLevelPack.hpp"

namespace PlaylistCore {
    namespace Utils {

        std::string GetLevelHash(GlobalNamespace::BeatmapLevel* level);

        bool IsWipLevel(GlobalNamespace::BeatmapLevel* level);

        void RemoveAllBMBFSuffixes();

        std::string SanitizeFileName(std::string_view fileName);

        bool UniqueFileName(std::string_view fileName, std::string_view compareDirectory);

        std::string GetNewPlaylistPath(std::string_view title);

        std::string GetBase64ImageType(std::string_view base64);

        std::string ProcessImage(UnityEngine::Texture2D* texture, bool returnPngString);

        void WriteImageToFile(std::string_view pathToPng, UnityEngine::Texture2D* texture);

        List<GlobalNamespace::BeatmapLevelPack*>* GetCustomPacks();

        void SetCustomPacks(List<GlobalNamespace::BeatmapLevelPack*>* newPlaylists, bool updateSongs);
    }
}
