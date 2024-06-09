#pragma once

#include "GlobalNamespace/BeatmapLevel.hpp"
#include "GlobalNamespace/BeatmapLevelPack.hpp"
#include "UnityEngine/Texture2D.hpp"

namespace PlaylistCore {
    namespace Utils {
        bool CaseInsensitiveEquals(std::string const& a, std::string const& b);

        GlobalNamespace::BeatmapLevel* GetLevelByID(std::string id);

        std::string GetLevelHash(std::string id);

        std::string GetLevelHash(GlobalNamespace::BeatmapLevel* level);

        bool IsWipLevel(GlobalNamespace::BeatmapLevel* level);

        void RemoveAllBMBFSuffixes();

        std::string SanitizeFileName(std::string_view fileName);

        bool UniqueFileName(std::string_view fileName, std::string_view compareDirectory);

        std::string GetNewPlaylistPath(std::string_view title);

        std::string GetBase64ImageType(std::string_view base64);

        std::string ProcessImage(UnityEngine::Texture2D* texture, bool returnPngString);

        void WriteImageToFile(std::string_view pathToPng, UnityEngine::Texture2D* texture);
    }
}
