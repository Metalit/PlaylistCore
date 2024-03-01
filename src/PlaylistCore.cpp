#include "Main.hpp"
#include "Types/BPList.hpp"
#include "Types/Config.hpp"
#include "PlaylistCore.hpp"
#include "ResettableStaticPtr.hpp"
#include "SpriteCache.hpp"
#include "Settings.hpp"
#include "Utils.hpp"
#include "Backup.hpp"

#include <filesystem>
#include <fstream>
#include <thread>
#include <map>

#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"

#include "bsml/shared/BSML/MainThreadScheduler.hpp"

#include "songdownloader/shared/BeatSaverAPI.hpp"

#include "songloader/shared/API.hpp"

#include "System/Convert.hpp"
#include "UnityEngine/Resources.hpp"
#include "UnityEngine/ImageConversion.hpp"
#include "UnityEngine/SpriteMeshType.hpp"
#include "UnityEngine/TextureFormat.hpp"
#include "GlobalNamespace/CustomLevelLoader.hpp"
#include "GlobalNamespace/CustomPreviewBeatmapLevel.hpp"
#include "GlobalNamespace/IBeatmapLevelCollection.hpp"

using namespace RuntimeSongLoader;
using namespace PlaylistCore::Utils;

namespace PlaylistCore {

    std::map<std::string, Playlist*> path_playlists;
    std::unordered_map<UnityEngine::Sprite*, std::string> image_paths;

    // array of all loaded images
    std::vector<UnityEngine::Sprite*> loadedImages;

    bool hasLoaded = false;
    // all unusable playlists
    std::unordered_set<std::string> staticPackIDs{};
    // playlists that need to be reloaded on the next reload
    std::unordered_set<Playlist*> needsReloadPlaylists{};
    // functions that filter out playlists from being shown
    std::vector<std::pair<modloader::ModInfo, std::function<bool(std::string const& path)>>> playlistFilters;

    void Playlist::Save() {
        if(!WriteToFile(path, playlistJSON) || !WriteToFile(GetPlaylistBackupPath(path), playlistJSON))
            LOG_ERROR("Error saving playlist! Path: %s", path.c_str());
    }

    UnityEngine::Sprite* GetDefaultCoverImage() {
        return FindComponent<GlobalNamespace::CustomLevelLoader*>()->_defaultPackCover;
    }

    UnityEngine::Sprite* GetCoverImage(Playlist* playlist) {
        // changes to playlist cover should change index as well
        if(playlist->imageIndex >= 0)
            return loadedImages[playlist->imageIndex];
        // index is -1 with unloaded or default cover image
        auto& json = playlist->playlistJSON;
        if(json.ImageString.has_value()) {
            std::string_view imageBase64 = json.ImageString.value();
            // trim "data:image/png;base64,"-like metadata
            static std::string searchString = "base64,";
            // only search first ~20 characters
            auto searchIndex = imageBase64.substr(0, 40).find(searchString);
            if(searchIndex != std::string::npos)
                imageBase64 = imageBase64.substr(searchIndex + searchString.length());
            // return loaded image if existing
            if(auto sprite = HasCachedSprite(imageBase64)) {
                LOG_INFO("Returning loaded image");
                int index = std::find(loadedImages.begin(), loadedImages.end(), sprite) - loadedImages.begin();
                playlist->imageIndex = index;
                return sprite;
            }
            // check image type
            std::string imgType = GetBase64ImageType(imageBase64);
            if(imgType != ".png" && imgType != ".jpg") {
                LOG_ERROR("Unsupported image type %s", imgType.c_str());
                return GetDefaultCoverImage();
            }
            // get and write texture
            auto texture = UnityEngine::Texture2D::New_ctor(0, 0, UnityEngine::TextureFormat::RGBA32, false, false);
            LOG_INFO("Loading image of playlist %s", playlist->name.c_str());
            try {
                UnityEngine::ImageConversion::LoadImage(texture, System::Convert::FromBase64String(imageBase64)); // copy
            } catch (std::exception const& exc) {
                LOG_DEBUG("Error loading image: %s", exc.what());
                return GetDefaultCoverImage();
            }
            // process texture size and png string and check hash for changes
            auto newImageBase64 = ProcessImage(texture, true); // probably most expensive idk
            // write to playlist if changed
            if(newImageBase64 != imageBase64) {
                json.ImageString = newImageBase64; // copy
                playlist->Save();
            }
            if(auto sprite = HasCachedSprite(newImageBase64)) {
                LOG_INFO("Returning loaded image");
                int index = std::find(loadedImages.begin(), loadedImages.end(), sprite) - loadedImages.begin();
                playlist->imageIndex = index;
                return sprite;
            }
            // save image as file with playlist file name and return
            std::string playlistPathName = std::filesystem::path(playlist->path).stem();
            std::string imgPath = GetCoversPath() + "/" + playlistPathName + ".png";
            LOG_INFO("Writing image from playlist to %s", imgPath.c_str());
            WriteImageToFile(imgPath, texture);
            auto sprite = UnityEngine::Sprite::Create(texture, UnityEngine::Rect(0, 0, texture->get_width(), texture->get_height()), {0.5, 0.5}, 1024, 1, UnityEngine::SpriteMeshType::FullRect, {0, 0, 0, 0}, false);
            CacheSprite(sprite, std::move(newImageBase64));
            image_paths.insert({sprite, imgPath});
            playlist->imageIndex = loadedImages.size();
            loadedImages.emplace_back(sprite);
            return sprite;
        }
        playlist->imageIndex = -1;
        return GetDefaultCoverImage();
    }

    void DeleteLoadedImage(int index) {
        auto sprite = loadedImages[index];
        // get path
        auto pathItr = image_paths.find(loadedImages[index]);
        if (pathItr == image_paths.end())
            return;
        // update image indices of playlists
        for(auto& playlist : GetLoadedPlaylists()) {
            if(playlist->imageIndex == index)
                playlist->imageIndex = -1;
            if(playlist->imageIndex > index)
                playlist->imageIndex--;
        }
        // remove from path map and delete file
        std::filesystem::remove(pathItr->second);
        image_paths.erase(sprite);
        // remove from loaded images
        loadedImages.erase(loadedImages.begin() + index);
        RemoveCachedSprite(sprite);
    }

    void LoadCoverImages() {
        // ensure path exists
        auto imagePath = GetCoversPath();
        if(!std::filesystem::is_directory(imagePath))
            return;
        // iterate through all image files
        for(const auto& file : std::filesystem::directory_iterator(imagePath)) {
            if(!file.is_directory()) {
                auto path = file.path();
                std::string extension = path.extension().string();
                LOWER(extension);
                // check file extension
                if(extension == ".jpg") {
                    auto newPath = path.parent_path() / (path.stem().string() + ".png");
                    std::filesystem::rename(path, newPath);
                    path = newPath;
                } else if(extension != ".png") {
                    LOG_ERROR("Incompatible file extension: %s", extension.c_str());
                    continue;
                }
                // check hash of base image before converting to sprite and to png
                std::ifstream instream(path, std::ios::in | std::ios::binary | std::ios::ate);
                auto size = instream.tellg();
                instream.seekg(0, instream.beg);
                auto bytes = Array<uint8_t>::NewLength(size);
                instream.read(reinterpret_cast<char*>(bytes->_values), size);
                std::string imageString = System::Convert::ToBase64String(bytes);
                if(HasCachedSprite(imageString)) {
                    LOG_INFO("Skipping loading image %s", path.string().c_str());
                    continue;
                }
                // sanatize hash by converting to png
                auto texture = UnityEngine::Texture2D::New_ctor(0, 0, UnityEngine::TextureFormat::RGBA32, false, false);
                try {
                    UnityEngine::ImageConversion::LoadImage(texture, bytes);
                } catch (std::exception const& exc) {
                    LOG_DEBUG("Error loading image: %s", exc.what());
                    continue;
                }
                std::string newImageString = ProcessImage(texture, true);
                if(newImageString != imageString)
                    WriteImageToFile(path.string(), texture);
                // check hash with loaded images
                if(HasCachedSprite(newImageString)) {
                    LOG_INFO("Skipping loading image %s", path.string().c_str());
                    continue;
                }
                LOG_INFO("Loading image %s", path.string().c_str());
                auto sprite = UnityEngine::Sprite::Create(texture, UnityEngine::Rect(0, 0, texture->get_width(), texture->get_height()), {0.5, 0.5}, 1024, 1, UnityEngine::SpriteMeshType::FullRect, {0, 0, 0, 0}, false);
                CacheSprite(sprite, std::move(newImageString));
                image_paths.insert({sprite, path});
                loadedImages.emplace_back(sprite);
            }
        }
    }

    std::vector<UnityEngine::Sprite*> const& GetLoadedImages() {
        return loadedImages;
    }

    void ClearLoadedImages() {
        loadedImages.clear();
        image_paths.clear();
        ClearCachedSprites();
    }

    void LoadPlaylists(SongLoaderBeatmapLevelPackCollectionSO* customBeatmapLevelPackCollectionSO, bool fullReload) {
        LOG_INFO("Loading playlists");
        RemoveAllBMBFSuffixes();
        LoadCoverImages(); // can be laggy depending on the number of images, but generally only loads a lot on launch when the screen is black anyway
        if(auto func = GetBackupFunction()) {
            LOG_INFO("Showing backup dialog");
            ShowBackupDialog(func);
            hasLoaded = true;
            return;
        }
        // clear playlists if requested
        if(fullReload) {
            for(auto& pair : path_playlists)
                MarkPlaylistForReload(pair.second);
        }
        // ensure path exists
        auto path = GetPlaylistsPath();
        if(!std::filesystem::exists(path) || !std::filesystem::is_directory(path))
            return;
        // clear out old playlists if showDefaults is off
        if(!IsPlaylistShown("Defaults")) {
            LOG_INFO("Removing default playlists from being shown");
            GlobalNamespace::CustomBeatmapLevelPack *customsPack = nullptr, *customWIPsPack = nullptr;
            for(auto& pack : customBeatmapLevelPackCollectionSO->customBeatmapLevelPacks->_items) {
                if(!pack)
                    continue;
                if(pack->get_packName() == "Custom Levels")
                    customsPack = pack;
                if(pack->get_packName() == "WIP Levels")
                    customWIPsPack = pack;
            }
            if(customsPack)
                customBeatmapLevelPackCollectionSO->RemoveLevelPack(customsPack);
            if(customWIPsPack)
                customBeatmapLevelPackCollectionSO->RemoveLevelPack(customWIPsPack);
        }
        // create set of playlists that aren't found when loading
        std::unordered_set<std::string> removedPaths{};
        auto orderVec = getConfig().Order.GetValue();
        for(auto& path : orderVec)
            removedPaths.insert(path);
        // create array for playlists
        std::vector<GlobalNamespace::CustomBeatmapLevelPack*> sortedPlaylists(orderVec.size());
        // iterate through all playlist files
        for(const auto& entry : std::filesystem::directory_iterator(path)) {
            if(!entry.is_directory()) {
                Playlist* playlist = nullptr;
                // check if playlist has been loaded already
                auto path = entry.path().string();
                auto path_iter = path_playlists.find(path);
                if(path_iter != path_playlists.end())
                    playlist = path_iter->second;
                // load from cache without reload
                if(playlist && !needsReloadPlaylists.contains(playlist)) {
                    LOG_INFO("Loading playlist file %s from cache", path.c_str());
                    // check if playlist should be added
                    // check if playlist needs to be reloaded
                    if(IsPlaylistShown(playlist->path)) {
                        int packPosition = GetPlaylistIndex(playlist->path);
                        // add if new (idk how)
                        if(packPosition < 0)
                            sortedPlaylists.emplace_back(playlist->playlistCS);
                        else
                            sortedPlaylists[packPosition] = (GlobalNamespace::CustomBeatmapLevelPack*) playlist->playlistCS;
                    }
                } else {
                    LOG_INFO("Loading playlist file %s", path.c_str());
                    // only create a new playlist if one doesn't exist
                    // if one does, its contents will simply be overwritten with the reloaded data
                    if(!playlist)
                        playlist = new Playlist();
                    else {
                        needsReloadPlaylists.erase(playlist);
                        // clear cached data in playlist object
                        playlist->imageIndex = -1;
                    }
                    // get playlist object from file
                    bool success = false;
                    try {
                        ReadFromFile(path, playlist->playlistJSON);
                        success = true;
                    } catch(const std::exception& err) {
                        LOG_ERROR("Error loading playlist %s: %s", path.c_str(), err.what());
                        success = false;
                    }
                    if(success) {
                        playlist->name = playlist->playlistJSON.PlaylistTitle;
                        playlist->path = path;
                        path_playlists.insert({playlist->path, playlist});
                        // create playlist object
                        SongLoaderCustomBeatmapLevelPack* songloaderBeatmapLevelPack = SongLoaderCustomBeatmapLevelPack::Make_New(playlist->path, playlist->name, GetCoverImage(playlist));
                        playlist->playlistCS = songloaderBeatmapLevelPack->CustomLevelsPack;
                        // clear out duplicate songs
                        auto& songs = playlist->playlistJSON.Songs;
                        std::unordered_set<std::string> hashes{};
                        // add all songs to the playlist object
                        auto foundSongs = ListW<GlobalNamespace::CustomPreviewBeatmapLevel*>::New();
                        for(auto itr = songs.begin(); itr != songs.end(); itr++) {
                            LOWER(itr->Hash);
                            if(hashes.contains(itr->Hash)) {
                                songs.erase(itr);
                                itr--;
                            } else {
                                hashes.insert(itr->Hash);
                                auto search = RuntimeSongLoader::API::GetLevelByHash(itr->Hash);
                                if(search.has_value())
                                    foundSongs->Add(search.value());
                            }
                        }
                        // save removed duplicates
                        playlist->Save();
                        songloaderBeatmapLevelPack->SetCustomPreviewBeatmapLevels(foundSongs->ToArray());
                        // add the playlist to the sorted array
                        if(IsPlaylistShown(playlist->path)) {
                            int packPosition = GetPlaylistIndex(playlist->path);
                            // add if new
                            if(packPosition < 0)
                                sortedPlaylists.emplace_back(songloaderBeatmapLevelPack->CustomLevelsPack);
                            else
                                sortedPlaylists[packPosition] = songloaderBeatmapLevelPack->CustomLevelsPack;
                        }
                    } else {
                        delete playlist;
                        playlist = nullptr;
                    }
                }
                // keep path in order config if loaded
                if(playlist && removedPaths.contains(path))
                    removedPaths.erase(path);
            }
        }
        // add playlists to game in sorted order
        for(auto customBeatmapLevelPack : sortedPlaylists) {
            if(customBeatmapLevelPack)
                customBeatmapLevelPackCollectionSO->AddLevelPack(customBeatmapLevelPack);
        }
        // remove paths in order config that were not loaded
        for(auto& path : removedPaths) {
            for(auto iter = orderVec.begin(); iter != orderVec.end(); iter++) {
                if(*iter == path) {
                    orderVec.erase(iter);
                    iter--;
                }
            }
            // delete them if they were still loaded as well
            auto pathItr = path_playlists.find(path);
            if(pathItr != path_playlists.end()) {
                delete pathItr->second;
                path_playlists.erase(pathItr);
            }
        }
        getConfig().Order.SetValue(orderVec);
        hasLoaded = true;
        LOG_INFO("Playlists loaded");
    }

    std::vector<Playlist*> GetLoadedPlaylists() {
        // create return vector with base size
        std::vector<Playlist*> playlistArray(getConfig().Order.GetValue().size());
        for(auto& pair : path_playlists) {
            auto& playlist = pair.second;
            int idx = GetPlaylistIndex(playlist->path);
            if(idx >= 0)
                playlistArray[idx] = playlist;
            else
                playlistArray.push_back(playlist);
        }
        // remove empty slots
        for(auto itr = playlistArray.begin(); itr != playlistArray.end(); itr++) {
            if(*itr == nullptr) {
                playlistArray.erase(itr);
                itr--;
            }
        }
        return playlistArray;
    }

    Playlist* GetPlaylist(std::string const& path) {
        auto iter = path_playlists.find(path);
        if(iter == path_playlists.end())
            return nullptr;
        return iter->second;
    }

    Playlist* GetPlaylistWithPrefix(std::string const& id) {
        static const int prefixLength = std::string(CustomLevelPackPrefixID).length();
        if(id.starts_with(CustomLevelPackPrefixID))
            return GetPlaylist(id.substr(prefixLength));
        return nullptr;
    }

    int GetPlaylistIndex(std::string const& path) {
        auto orderVec = getConfig().Order.GetValue();
        // find index of playlist title in config
        for(int i = 0; i < orderVec.size(); i++) {
            if(orderVec[i] == path)
                return i;
        }
        // add to end of config if not found
        orderVec.push_back(path);
        getConfig().Order.SetValue(orderVec);
        return -1;
    }

    bool IsPlaylistShown(std::string const& path) {
        bool shown = true;
        for(auto& pair : playlistFilters)
            shown = shown && pair.second(path);
        return shown;
    }

    void AddPlaylistFilter(modloader::ModInfo mod, std::function<bool(std::string const& path)> func) {
        playlistFilters.emplace_back(std::make_pair(mod, func));
    }

    void RemovePlaylistFilters(modloader::ModInfo mod) {
        for(auto itr = playlistFilters.begin(); itr != playlistFilters.end(); itr++) {
            if(itr->first.id == mod.id && itr->first.version == mod.version) {
                playlistFilters.erase(itr);
                itr--;
            }
        }
    }

    std::string AddPlaylist(std::string const& title, std::string const& author, UnityEngine::Sprite* coverImage) {
        // create playlist with info
        auto newPlaylist = BPList();
        newPlaylist.PlaylistTitle = title;
        if(author != "")
            newPlaylist.PlaylistAuthor = author;
        if(coverImage) {
            auto bytes = UnityEngine::ImageConversion::EncodeToPNG(coverImage->get_texture());
            newPlaylist.ImageString = System::Convert::ToBase64String(bytes);
        }
        // save playlist
        std::string path = GetNewPlaylistPath(title);
        WriteToFile(path, newPlaylist);
        // update backups
        WriteToFile(GetPlaylistBackupPath(path), newPlaylist);
        return path;
    }

    std::pair<std::string, Playlist*> AddPlaylist(BPList playlist, bool reloadPlaylists) {
        // save playlist
        std::string path = GetNewPlaylistPath(playlist.PlaylistTitle);
        WriteToFile(path, playlist);
        // update backups
        WriteToFile(GetPlaylistBackupPath(path), playlist);
        Playlist* ret = nullptr;
        if (reloadPlaylists) {
            ReloadPlaylists();
            ret = GetPlaylist(path);
        }
        return {path, ret};
    }

    void MovePlaylist(Playlist* playlist, int index) {
        int originalIndex = GetPlaylistIndex(playlist->path);
        if(originalIndex < 0) {
            LOG_ERROR("Attempting to move unloaded playlist");
            return;
        }
        auto orderVec = getConfig().Order.GetValue();
        orderVec.erase(orderVec.begin() + originalIndex);
        orderVec.insert(orderVec.begin() + index, playlist->path);
        getConfig().Order.SetValue(orderVec);
    }

    void RenamePlaylist(Playlist* playlist, std::string const& title) {
        // edit variables
        playlist->name = title;
        playlist->playlistJSON.PlaylistTitle = title;
        // rename playlist ingame
        auto& levelPack = playlist->playlistCS;
        if(levelPack) {
            levelPack->____packName_k__BackingField = title;
            levelPack->____shortPackName_k__BackingField = title;
        }
        // save changes
        playlist->Save();
    }

    void ChangePlaylistCover(Playlist* playlist, int index) {
        UnityEngine::Sprite* newCover = nullptr;
        // update json image string
        auto& json = playlist->playlistJSON;
        if(index < 0) {
            newCover = GetDefaultCoverImage();
            // don't save string for default cover
            json.ImageString = std::nullopt;
        } else {
            newCover = GetLoadedImages()[index];
            // save image base 64
            auto bytes = UnityEngine::ImageConversion::EncodeToPNG(newCover->get_texture());
            json.ImageString = System::Convert::ToBase64String(bytes);
        }
        playlist->imageIndex = index;
        // change cover ingame
        auto& levelPack = playlist->playlistCS;
        if(levelPack) {
            levelPack->____coverImage_k__BackingField = newCover;
            levelPack->____smallCoverImage_k__BackingField = newCover;
        }
        playlist->Save();
    }

    void DeletePlaylist(Playlist* playlist) {
        // remove from map
        auto path_iter = path_playlists.find(playlist->path);
        if(path_iter == path_playlists.end()) {
            LOG_ERROR("Could not find playlist by path");
            return;
        }
        path_playlists.erase(path_iter);
        // delete file
        std::filesystem::remove(playlist->path);
        std::filesystem::remove(GetPlaylistBackupPath(playlist->path));
        // remove name from order config
        int orderIndex = GetPlaylistIndex(playlist->path);
        auto orderVec = getConfig().Order.GetValue();
        if(orderIndex >= 0)
            orderVec.erase(orderVec.begin() + orderIndex);
        else
            orderVec.erase(orderVec.end() - 1);
        getConfig().Order.SetValue(orderVec);
        // delete playlist object
        delete playlist;
    }

    void ReloadPlaylists(bool fullReload) {
        if(!hasLoaded)
            return;
        // handle full reload here since songloader's full refesh isn't carried through
        // also, we don't want to always full reload songs at the same time as playlists
        if(fullReload) {
            for(auto& pair : path_playlists)
                MarkPlaylistForReload(pair.second);
        }
        API::RefreshPacks();
    }

    void MarkPlaylistForReload(Playlist* playlist) {
        needsReloadPlaylists.insert(playlist);
    }

    int PlaylistHasMissingSongs(Playlist* playlist) {
        int songsMissing = 0;
        for(auto& song : playlist->playlistJSON.Songs) {
            std::string& hash = song.Hash;
            LOWER(hash);
            bool hasSong = false;
            // search in songs in playlist instead of all songs
            // we need to treat the list as an array because it is initialized as an array elsewhere
            ArrayW<GlobalNamespace::IPreviewBeatmapLevel*> levelList(playlist->playlistCS->beatmapLevelCollection->get_beatmapLevels());
            for(int i = 0; i < levelList.size(); i++) {
                if(hash == GetLevelHash(levelList[i])) {
                    hasSong = true;
                    break;
                }
            }
            if(hasSong)
                continue;
            songsMissing += 1;
        }
        return songsMissing;
    }

    void DownloadMissingSongsFromPlaylist(Playlist* playlist, std::function<void()> finishCallback, std::function<void(int, int)> updateCallback) {
        // find number of songs that need to be in the queue before downloading
        int quantity = PlaylistHasMissingSongs(playlist);
        if(quantity == 0) {
            if(finishCallback)
                finishCallback();
            return;
        }
        if(updateCallback)
            updateCallback(0, quantity);
        // queue songs
        auto songQueue = std::vector<std::string>{};
        // track actual downloads
        auto downloads = new std::atomic_int(0);
        // add all nonpresent song hashes to queue
        for(auto& song : playlist->playlistJSON.Songs) {
            std::string& hash = song.Hash;
            LOWER(hash);
            bool hasSong = false;
            // search in songs in playlist instead of all songs
            // we need to treat the list as an array because it is initialized as an array elsewhere
            ArrayW<GlobalNamespace::IPreviewBeatmapLevel*> levelList(playlist->playlistCS->beatmapLevelCollection->get_beatmapLevels());
            for(int i = 0; i < levelList.size(); i++) {
                if(hash == GetLevelHash(levelList[i])) {
                    hasSong = true;
                    break;
                }
            }
            if(!hasSong)
                songQueue.emplace_back(hash);
        }
        // recursive (because threads) callback for each time a beatmap is recieved from beatsaver
        static void(*onBeatmap)(std::vector<std::string>, std::optional<BeatSaver::Beatmap>, std::function<void()>, std::function<void(int, int)>, int, std::atomic_int*)
                = *[](std::vector<std::string> songQueue, std::optional<BeatSaver::Beatmap> beatmap, std::function<void()> finishCallback, std::function<void(int, int)> updateCallback, int quantity, std::atomic_int* downloads) mutable {
            // start next beatmap
            if(!songQueue.empty()) {
                BeatSaver::API::GetBeatmapByHashAsync(songQueue.back(), [songQueue = std::move(songQueue), finishCallback, updateCallback, quantity, downloads](std::optional<BeatSaver::Beatmap> beatmap) mutable {
                    songQueue.pop_back();
                    onBeatmap(std::move(songQueue), std::move(beatmap), finishCallback, updateCallback, quantity, downloads);
                });
            }
            // download if beatmap is found, but update downloads and potentially run finish either way
            if(beatmap.has_value()) {
                BeatSaver::API::DownloadBeatmapAsync(beatmap.value(), [finishCallback, updateCallback, quantity, downloads](bool _) {
                    bool complete = (*downloads)++ == quantity - 1;
                    if(updateCallback)
                        BSML::MainThreadScheduler::Schedule([updateCallback, quantity, downloads]() { updateCallback(*downloads, quantity); });
                    if(complete && finishCallback) {
                        BSML::MainThreadScheduler::Schedule(finishCallback);
                        delete downloads;
                    }
                });
            } else {
                LOG_INFO("Beatmap not found on beatsaver");
                bool complete = (*downloads)++ == quantity - 1;
                if(updateCallback)
                    BSML::MainThreadScheduler::Schedule([updateCallback, quantity, downloads]() { updateCallback(*downloads, quantity); });
                if(complete && finishCallback) {
                    BSML::MainThreadScheduler::Schedule(finishCallback);
                    delete downloads;
                }
            }
        };
        BeatSaver::API::GetBeatmapByHashAsync(songQueue.back(), [songQueue = std::move(songQueue), finishCallback, updateCallback, quantity, downloads](std::optional<BeatSaver::Beatmap> beatmap) mutable {
            songQueue.pop_back();
            onBeatmap(std::move(songQueue), std::move(beatmap), finishCallback, updateCallback, quantity, downloads);
        });
    }

    void RemoveMissingSongsFromPlaylist(Playlist* playlist) {
        // store exisiting songs in a new vector to replace the song list with
        std::vector<BPSong> existingSongs = {};
        for(auto& song : playlist->playlistJSON.Songs) {
            std::string& hash = song.Hash;
            if(RuntimeSongLoader::API::GetLevelByHash(hash).has_value())
                existingSongs.push_back(song);
            else if(song.SongName.has_value())
                LOG_INFO("Removing song %s from playlist %s", song.SongName.value().c_str(), playlist->name.c_str());
            else
                LOG_INFO("Removing song with hash %s from playlist %s", hash.c_str(), playlist->name.c_str());
        }
        // set the songs of the playlist to only those found
        playlist->playlistJSON.Songs = existingSongs;
        playlist->Save();
    }

    void AddSongToPlaylist(Playlist* playlist, GlobalNamespace::IPreviewBeatmapLevel* level) {
        if(!level)
            return;
        // add song to cs object
        auto& pack = playlist->playlistCS;
        if(!pack)
            return;
        ArrayW<GlobalNamespace::IPreviewBeatmapLevel*> levelList(pack->beatmapLevelCollection->get_beatmapLevels());
        ArrayW<GlobalNamespace::IPreviewBeatmapLevel*> newLevels(levelList.size() + 1);
        for(int i = 0; i < levelList.size(); i++) {
            auto currentLevel = levelList[i];
            if(currentLevel->get_levelID() == level->get_levelID())
                return;
            newLevels[i] = currentLevel;
        }
        newLevels[levelList.size()] = level;
        auto readOnlyList = (System::Collections::Generic::IReadOnlyList_1<GlobalNamespace::CustomPreviewBeatmapLevel*>*) newLevels.convert();
        ((GlobalNamespace::CustomBeatmapLevelCollection*) pack->beatmapLevelCollection)->_customPreviewBeatmapLevels = readOnlyList;
        // update json object
        auto& json = playlist->playlistJSON;
        // add a blank song
        json.Songs.emplace_back(BPSong());
        // set info
        auto& songJson = *(json.Songs.end() - 1);
        songJson.Hash = GetLevelHash(level);
        songJson.SongName = level->get_songName();
        // write to file
        playlist->Save();
    }

    void RemoveSongFromPlaylist(Playlist* playlist, GlobalNamespace::IPreviewBeatmapLevel* level) {
        if(!level)
            return;
        // remove song from cs object
        auto& pack = playlist->playlistCS;
        if(!pack)
            return;
        ArrayW<GlobalNamespace::IPreviewBeatmapLevel*> levelList(pack->beatmapLevelCollection->get_beatmapLevels());
        if(levelList.size() == 0)
            return;
        ArrayW<GlobalNamespace::IPreviewBeatmapLevel*> newLevels(levelList.size() - 1);
        // remove only one level if duplicates
        bool removed = false;
        for(int i = 0; i < newLevels.size(); i++) {
            // comparison should work
            auto currentLevel = levelList[removed ? i + 1 : i];
            if(currentLevel->get_levelID() == level->get_levelID()) {
                removed = true;
                i--;
            } else
                newLevels[i] = currentLevel;
        }
        if(!removed && levelList->Last()->get_levelID() != level->get_levelID()) {
            LOG_ERROR("Could not find song to be removed!");
            return;
        }
        auto readOnlyList = (System::Collections::Generic::IReadOnlyList_1<GlobalNamespace::CustomPreviewBeatmapLevel*>*) newLevels.convert();
        ((GlobalNamespace::CustomBeatmapLevelCollection*) pack->beatmapLevelCollection)->_customPreviewBeatmapLevels = readOnlyList;
        // update json object
        auto& json = playlist->playlistJSON;
        // find song by hash (since the field is required) and remove
        auto levelHash = GetLevelHash(level);
        for(auto itr = json.Songs.begin(); itr != json.Songs.end(); ++itr) {
            auto& song = *itr;
            LOWER(song.Hash);
            if(song.Hash == levelHash) {
                json.Songs.erase(itr);
                // only erase
                break;
            }
        }
        // write to file
        playlist->Save();
    }

    void RemoveSongFromAllPlaylists(GlobalNamespace::IPreviewBeatmapLevel* level) {
        for(auto& pair : path_playlists)
            RemoveSongFromPlaylist(pair.second, level);
    }

    void SetSongIndex(Playlist* playlist, GlobalNamespace::IPreviewBeatmapLevel* level, int index) {
        if(!level)
            return;
        // remove song from cs object
        auto& pack = playlist->playlistCS;
        if(!pack)
            return;
        ArrayW<GlobalNamespace::IPreviewBeatmapLevel*> levelList(pack->beatmapLevelCollection->get_beatmapLevels());
        if(index >= levelList.size() || index < 0)
            return;
        ArrayW<GlobalNamespace::IPreviewBeatmapLevel*> newLevels(levelList.size());
        bool found = false;
        // ensure we traverse the whole of both lists
        for(int i = 0, j = 0; i < newLevels.size() || j < levelList.size(); i++) {
            // skip past level in original list, but only the first time
            if(j < levelList.size() && levelList[j]->get_levelID() == level->get_levelID() && !found) {
                j++;
                found = true;
            }
            // shift backwards in original list when inserting level
            if(i == index) {
                j--;
                newLevels[i] = level;
            } else if(i < newLevels.size()) {
                newLevels[i] = levelList[j];
            }
            j++;
        }
        if(found) {
            auto readOnlyList = (System::Collections::Generic::IReadOnlyList_1<GlobalNamespace::CustomPreviewBeatmapLevel*>*) newLevels.convert();
            ((GlobalNamespace::CustomBeatmapLevelCollection*) pack->beatmapLevelCollection)->_customPreviewBeatmapLevels = readOnlyList;
        } else {
            LOG_ERROR("Could not find song to be moved!");
            return;
        }
        // update json object
        auto& json = playlist->playlistJSON;
        // find song by hash (since the field is required) and remove
        auto levelHash = GetLevelHash(level);
        for(auto itr = json.Songs.begin(); itr != json.Songs.end(); ++itr) {
            auto& song = *itr;
            LOWER(song.Hash);
            if(song.Hash == levelHash) {
                json.Songs.erase(itr);
                // only erase
                break;
            }
        }
        // add a blank song
        json.Songs.insert(json.Songs.begin() + index, BPSong());
        // set info
        auto& songJson = json.Songs[index];
        songJson.Hash = GetLevelHash(level);
        songJson.SongName = level->get_songName();
        // write to file
        playlist->Save();
    }
}
