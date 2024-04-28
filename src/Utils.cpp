#include "Main.hpp"
#include "Utils.hpp"
#include "PlaylistCore.hpp"
#include "ResettableStaticPtr.hpp"

#include "songcore/shared/SongCore.hpp"
#include "bsml/shared/Helpers/getters.hpp"

#include "UnityEngine/ImageConversion.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/RectTransform.hpp"
#include "TMPro/TextMeshProUGUI.hpp"
#include "System/Convert.hpp"
#include "System/Action_4.hpp"
#include "System/Collections/Generic/IReadOnlyList_1.hpp"
#include "GlobalNamespace/LevelFilteringNavigationController.hpp"
#include "GlobalNamespace/LevelCollectionNavigationController.hpp"
#include "GlobalNamespace/LevelCollectionViewController.hpp"
#include "GlobalNamespace/LevelCollectionTableView.hpp"
#include "GlobalNamespace/LevelSelectionFlowCoordinator.hpp"
#include "GlobalNamespace/BeatmapLevelsRepository.hpp"
#include "GlobalNamespace/AnnotatedBeatmapLevelCollectionsGridView.hpp"
#include "GlobalNamespace/PageControl.hpp"
#include "GlobalNamespace/GridView.hpp"
#include "GlobalNamespace/AnnotatedBeatmapLevelCollectionsGridViewAnimator.hpp"
#include "GlobalNamespace/BeatmapLevelsModel.hpp"
#include "GlobalNamespace/LevelSearchViewController.hpp"

#include <filesystem>

namespace PlaylistCore {

    namespace Utils {

        // desired image size
        const int imageSize = 512;

        GlobalNamespace::BeatmapLevel* GetLevelByID(std::string id) {
            if(auto search = SongCore::API::Loading::GetLevelByLevelID(id))
                return search;
            else if(auto levels = BSML::Helpers::GetMainFlowCoordinator()->_beatmapLevelsModel)
                return levels->GetBeatmapLevel(id);
            return nullptr;
        }

        std::string GetLevelHash(std::string id) {
            // should be in all songloader levels
            auto prefixIndex = id.find("custom_level_");
            if(prefixIndex == std::string::npos)
                return "";
            // remove prefix
            id = id.substr(prefixIndex + 13);
            auto wipIndex = id.find(" WIP");
            if(wipIndex != std::string::npos)
                id = id.substr(0, wipIndex);
            LOWER(id);
            return id;
        }

        std::string GetLevelHash(GlobalNamespace::BeatmapLevel* level) {
            return GetLevelHash(level->levelID);
        }

        bool IsWipLevel(GlobalNamespace::BeatmapLevel* level) {
            return level->levelID.ends_with(" WIP");
        }

        void RemoveAllBMBFSuffixes() {
            static const std::string suffix("_BMBF.json");
            for(const auto& entry : std::filesystem::directory_iterator(GetPlaylistsPath())) {
                auto path = entry.path().string();
                while(path.ends_with(suffix))
                    path = path.substr(0, path.length() - suffix.length());
                std::filesystem::rename(entry.path(), path);
            }
        }

        std::string SanitizeFileName(std::string_view fileName) {
            std::string newName;
            // just whitelist simple characters
            static const auto okChar = [](unsigned char c) {
                if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) return true;
                if (c == '_' || c == '-' || c == '.' || c == '(' || c == ')') return true;
                return false;
            };
            std::transform(fileName.begin(), fileName.end(), std::back_inserter(newName), [](unsigned char c) {
                if(!okChar(c))
                    return (unsigned char)('_');
                return c;
            });
            if(newName == "")
                return "_";
            return newName;
        }

        bool UniqueFileName(std::string_view fileName, std::string_view compareDirectory) {
            if(!std::filesystem::is_directory(compareDirectory))
                return true;
            for(auto& entry : std::filesystem::directory_iterator(compareDirectory)) {
                if(entry.is_directory())
                    continue;
                if(entry.path().filename().string() == fileName)
                    return false;
            }
            return true;
        }

        std::string GetNewPlaylistPath(std::string_view title) {
            std::string fileTitle = SanitizeFileName(title);
            while(!UniqueFileName(fileTitle + ".bplist", GetPlaylistsPath()))
                fileTitle = "_" + fileTitle;
            return GetPlaylistsPath() + "/" + fileTitle + ".bplist";
        }

        std::string GetBase64ImageType(std::string_view base64) {
            if(base64.length() < 3)
                return "unknown";
            std::string_view sub = base64.substr(0, 3);
            if(sub == "iVB")
                return ".png";
            if(sub == "/9j")
                return ".jpg";
            if(sub == "R0l")
                return ".gif";
            if(sub == "Qk1")
                return ".bmp";
            return "unknown";
        }

        std::string ProcessImage(UnityEngine::Texture2D* texture, bool returnPngString) {
            // check texture size and resize if necessary
            int width = texture->get_width();
            int height = texture->get_height();
            if(width > imageSize && height > imageSize) {
                // resize (https://gist.github.com/gszauer/7799899 modified for only downscaling)
                auto texColors = texture->GetPixels();
                ArrayW<UnityEngine::Color> newColors(imageSize * imageSize);
                float ratio_x = ((float) width - 1) / imageSize;
                float ratio_y = ((float) height - 1) / imageSize;

                for(int y = 0; y < imageSize; y++) {
                    int offset_from_y = y * imageSize;

                    int old_texture_y = floor(y * ratio_y);
                    int old_texture_offset_from_y = old_texture_y * width;

                    for(int x = 0; x < imageSize; x++) {
                        int old_texture_x = floor(x * ratio_x);

                        newColors[offset_from_y + x] = texColors[old_texture_offset_from_y + old_texture_x];
                    }
                }
                texture->Resize(imageSize, imageSize);
                texture->SetPixels(newColors);
                texture->Apply();
            }
            if(!returnPngString)
                return "";
            // convert to png if necessary
            // can sometimes need two passes to reach a fixed result
            // but I don't want to to it twice for every cover, so it will just normalize itself after another restart
            // and it shouldn't be a problem through ingame cover management
            auto bytes = UnityEngine::ImageConversion::EncodeToPNG(texture);
            return System::Convert::ToBase64String(bytes);
        }

        void WriteImageToFile(std::string_view pathToPng, UnityEngine::Texture2D* texture) {
            auto bytes = UnityEngine::ImageConversion::EncodeToPNG(texture);
            writefile(pathToPng, std::string((char*) bytes.begin(), bytes.size()));
        }
    }
}
