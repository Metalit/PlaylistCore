#include "Main.hpp"
#include "PlaylistCore.hpp"
#include "Utils.hpp"
#include "SpriteCache.hpp"
#include "Backup.hpp"
#include "ResettableStaticPtr.hpp"

#include "Types/BPList.hpp"

#include "GlobalNamespace/SharedCoroutineStarter.hpp"
#include "UnityEngine/WaitForFixedUpdate.hpp"
#include "UnityEngine/ImageConversion.hpp"
#include "System/Convert.hpp"

#include "questui/shared/BeatSaberUI.hpp"
#include "custom-types/shared/coroutine.hpp"

#include <filesystem>

using namespace PlaylistCore;
using namespace QuestUI;

bool FixImageString(std::optional<std::string>& optional) {
    if(!optional.has_value())
        return false;
    bool changed = false;
    // trim "data:image/png;base64,"-like metadata
    std::string_view str = optional.value();
    static std::string searchString = "base64,";
    auto searchIndex = str.substr(0, 20).find(searchString);
    if(searchIndex != std::string::npos) {
        str = str.substr(searchIndex + searchString.length());
        changed |= true;
    }
    if(HasCachedSprite(str))
        return changed;
    // downscale image, texture should be cleaned up by unity sometime later
    auto texture = UnityEngine::Texture2D::New_ctor(0, 0, UnityEngine::TextureFormat::RGBA32, false, false);
    try {
        UnityEngine::ImageConversion::LoadImage(texture, System::Convert::FromBase64String(str));
    } catch (std::exception const& exc) {
        LOG_DEBUG("Error loading image: %s", exc.what());
        return changed;
    }
    std::string newStr = Utils::ProcessImage(texture, true);
    changed |= newStr != str;
    return changed;
}

bool LowerSongHashes(std::vector<PlaylistCore::BPSong>& songs) {
    bool changed = false;
    for(auto& song : songs) {
        std::string oldHash = song.Hash;
        LOWER(song.Hash);
        if(song.Hash != oldHash)
            changed = true;
    }
    return changed;
}

bool FixupPlaylist(BPList& playlist) {
    bool changed = false;
    // having bad image strings in backups can cause bad behavior
    if(FixImageString(playlist.ImageString)) {
        LOG_DEBUG("Fixing image string for playlist %s", playlist.PlaylistTitle.c_str());
        changed = true;
    }
    // you can't trust the case of these things
    if(LowerSongHashes(playlist.Songs)) {
        LOG_DEBUG("Fixing song hash case for playlist %s", playlist.PlaylistTitle.c_str());
        changed = true;
    }
    return changed;
}

// returns the match of a type in a list of it, or the search object if not found
template<class T>
T& IdentifyMatch(T& backup, std::vector<T>& objs);

BPSong& IdentifyMatch(BPSong& backup, std::vector<BPSong>& objs) {
    // LOWER(backup.Hash); // should be lowered by FixupPlaylist
    for(auto& obj : objs) {
        // LOWER(obj.Hash);
        if(obj.Hash == backup.Hash)
            return obj;
    }
    return backup;
}

using RestoreFunc = std::function<void()>;

// returns a function that will handle either restoring or not based on an object and a backup of it
template<class T>
bool ProcessBackup(T& obj, T& backup) {
    return false;
}

template<class V>
bool ProcessBackup(std::optional<V>& obj, std::optional<V>& backup) {
    if(!backup.has_value())
        return false;
    if(!obj.has_value()) {
        obj.emplace(backup.value());
        return true;
    }
    return ProcessBackup(obj.value(), backup.value());
}

template<>
bool ProcessBackup(BPSong& obj, BPSong& backup) {
    bool changed = false;
    // restore difficulties if removed
    if(ProcessBackup(obj.Difficulties, backup.Difficulties)) {
        changed = true;
        LOG_DEBUG("difficulties were restored from backup");
    }
    return changed;
}

template<>
bool ProcessBackup(CustomData& obj, CustomData& backup) {
    bool changed = false;
    // restore sync url if removed
    if(ProcessBackup(obj.SyncURL, backup.SyncURL)) {
        changed = true;
        LOG_DEBUG("sync url was restored from backup");
    }
    return changed;
}

template<>
bool ProcessBackup(BPList& obj, BPList& backup) {
    bool changed = false;
    // restore author if removed
    if(ProcessBackup(obj.PlaylistAuthor, backup.PlaylistAuthor)) {
        changed = true;
        LOG_DEBUG("author was restored from backup");
    }
    // restore description if removed
    if(ProcessBackup(obj.PlaylistDescription, backup.PlaylistDescription)) {
        changed = true;
        LOG_DEBUG("description was restored from backup");
    }
    // do backup restoration for all preserved songs
    if(obj.Songs != backup.Songs) {
        for(auto& song : obj.Songs) {
            auto& songBackup = IdentifyMatch(song, backup.Songs);
            // backup processor for two of the same object should be nothing
            if(ProcessBackup(song, songBackup)) {
                changed = true;
                // LOG_DEBUG("song data was restored from backup");
            }
        }
    }
    // restore customData if removed
    if(ProcessBackup(obj.CustomData, backup.CustomData)) {
        changed = true;
        LOG_DEBUG("custom data was restored from backup");
    }
    // restore image if removed
    if(ProcessBackup(obj.ImageString, backup.ImageString)) {
        changed = true;
        LOG_DEBUG("image was restored from backup");
    }
    return changed;
}

RestoreFunc GetBackupFunction() {
    // make backup if none exists
    if(std::filesystem::is_empty(GetBackupsPath())) {
        LOG_INFO("Creating backups");
        std::filesystem::remove(GetBackupsPath());
        std::filesystem::copy(GetPlaylistsPath(), GetBackupsPath());
        return nullptr;
    }
    // whether the playlists are different from those backed up
    bool changes = false;
    // get lists of names of playlist and backup files
    std::unordered_map<std::string, BPList> playlistPaths;
    std::unordered_map<std::string, BPList> backupPaths;
    for(const auto& entry : std::filesystem::directory_iterator(GetPlaylistsPath())) {
        // trim all but file name
        std::string path(entry.path().string());
        std::string pathTrimmed = path.substr(GetPlaylistsPath().length());
        // process any necessary fixes
        auto& json = playlistPaths.insert({pathTrimmed, BPList()}).first->second;
        try {
            ReadFromFile(path, json);
        } catch(const std::exception& err) {
            LOG_ERROR("Error loading playlist %s: %s", path.c_str(), err.what());
            continue;
        }
        if(FixupPlaylist(json))
            WriteToFile(path, json);
    }
    for(const auto& entry : std::filesystem::directory_iterator(GetBackupsPath())) {
        // trim all but file name
        std::string path(entry.path().string());
        std::string pathTrimmed = path.substr(GetBackupsPath().length());
        // process any necessary fixes
        auto& json = backupPaths.insert({pathTrimmed, BPList()}).first->second;
        try {
            ReadFromFile(path, json);
        } catch(const std::exception& err) {
            LOG_ERROR("Error loading playlist %s: %s", path.c_str(), err.what());
            continue;
        }
        if(FixupPlaylist(json))
            WriteToFile(path, json);
    }
    // get backup processors for all playlists present in both places
    int missing = 0;
    for(auto& pair : backupPaths) {
        const std::string& path = pair.first;
        auto currentPair = playlistPaths.find(path);
        if(currentPair != playlistPaths.end()) {
            LOG_DEBUG("comparing playlist %s", path.c_str());
            // load both into objects
            BPList& backupJson = pair.second;
            BPList& currentJson = currentPair->second;
            // process backup and make sure the playlist is reloaded if changed
            if(ProcessBackup(currentJson, backupJson)) {
                // LOG_DEBUG("playlist info restored from backup");
                WriteToFile(GetPlaylistsPath() + path, currentJson);
                // reload playlist if already loaded
                if(auto playlist = GetPlaylist(GetPlaylistsPath() + path)) {
                    MarkPlaylistForReload(playlist);
                }
            }
            if(currentJson != backupJson) {
                LOG_DEBUG("playlists were different");
                changes = true;
            }
        } else
            missing++; // keep track of number in backups not present in current
    }
    bool pathsMatch = backupPaths.size() - missing == playlistPaths.size();
    // add function to copy all playlists from backup if there are differences
    if(!pathsMatch || changes) {
        return [playlistPaths = std::move(playlistPaths), backupPaths = std::move(backupPaths)] {
            // copy and reload everything when restoring a backup
            std::filesystem::remove_all(GetPlaylistsPath());
            std::filesystem::copy(GetBackupsPath(), GetPlaylistsPath());
            for(auto& playlist : GetLoadedPlaylists()) {
                MarkPlaylistForReload(playlist);
            }
        };
    }
    return nullptr;
}

RestoreFunc backupFunction;

// significant credit for the ui to https://github.com/jk4837/PlaylistEditor/blob/master/src/Utils/UIUtils.cpp
HMUI::ModalView* MakeDialog() {
    auto parent = FindComponent<GlobalNamespace::MainMenuViewController*>()->get_transform();
    auto modal = BeatSaberUI::CreateModal(parent, {65, 41}, nullptr, false);

    static ConstString contentName("Content");

    auto restoreButton = BeatSaberUI::CreateUIButton(modal->get_transform(), "No", "ActionButton", {-16, -14}, [modal] {
        LOG_INFO("Restoring backup");
        modal->Hide(true, nullptr);
        backupFunction();
        ReloadPlaylists();
    });
    UnityEngine::Object::Destroy(restoreButton->get_transform()->Find(contentName)->GetComponent<UnityEngine::UI::LayoutElement*>());

    auto cancelButton = QuestUI::BeatSaberUI::CreateUIButton(modal->get_transform(), "Yes", {16, -14}, [modal] {
        modal->Hide(true, nullptr);
        std::filesystem::remove_all(GetBackupsPath());
        std::filesystem::copy(GetPlaylistsPath(), GetBackupsPath());
        ReloadPlaylists();
    });
    UnityEngine::Object::Destroy(cancelButton->get_transform()->Find(contentName)->GetComponent<UnityEngine::UI::LayoutElement*>());

    TMPro::TextMeshProUGUI* title = BeatSaberUI::CreateText(modal->get_transform(), "Playlist Core", false, {0, 16}, {60, 8.5});
    title->set_alignment(TMPro::TextAlignmentOptions::Center);
    title->set_fontStyle(TMPro::FontStyles::Bold);

    static ConstString dialogText("External playlist modifications detected (likely through BMBF). Changes made ingame may be lost. Would you like to keep the external changes?");

    TMPro::TextMeshProUGUI* message = QuestUI::BeatSaberUI::CreateText(modal->get_transform(), dialogText, false, {0, 2}, {60, 25.5});
    message->set_enableWordWrapping(true);
    message->set_alignment(TMPro::TextAlignmentOptions::Center);

    modal->get_transform()->SetAsLastSibling();
    return modal;
}

custom_types::Helpers::Coroutine ShowBackupDialogCoroutine() {
    auto mainViewController = FindComponent<GlobalNamespace::MainMenuViewController*>();
    while(!mainViewController->wasActivatedBefore)
        co_yield nullptr;
    
    STATIC_AUTO(modal, MakeDialog());
    modal->Show(true, true, nullptr);
}

void ShowBackupDialog(RestoreFunc backupFunc) {
    backupFunction = backupFunc;
    if(!backupFunction)
        return;
    GlobalNamespace::SharedCoroutineStarter::get_instance()->StartCoroutine(
        custom_types::Helpers::CoroutineHelper::New(ShowBackupDialogCoroutine()) );
}
