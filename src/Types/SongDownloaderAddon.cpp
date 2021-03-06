#include "Main.hpp"
#include "Types/SongDownloaderAddon.hpp"
#include "CustomTypes/CustomListSource.hpp"
#include "CustomTypes/CoverTableCell.hpp"
#include "PlaylistCore.hpp"

#include "questui/shared/BeatSaberUI.hpp"

#include "HMUI/Touchable.hpp"
#include "HMUI/ScrollView.hpp"
#include "HMUI/TableView_ScrollPositionType.hpp"
#include "System/Collections/Generic/HashSet_1.hpp"

DEFINE_TYPE(PlaylistCore, SongDownloaderAddon);

using namespace PlaylistCore;
using namespace QuestUI;

Playlist* SongDownloaderAddon::SelectedPlaylist = nullptr;

void SongDownloaderAddon::playlistSelected(int cellIdx) {
    selectedPlaylist = loadedPlaylists[cellIdx];
    if(downloadToPlaylistEnabled)
        SongDownloaderAddon::SelectedPlaylist = selectedPlaylist;
}

void SongDownloaderAddon::scrollListLeftButtonPressed() {
    CustomListSource::ScrollListLeft(list, 4);
}

void SongDownloaderAddon::scrollListRightButtonPressed() {
    CustomListSource::ScrollListRight(list, 4);
}

void SongDownloaderAddon::DidActivate(bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling) {

    if(!firstActivation) {
        RefreshPlaylists();
        return;
    }

    get_gameObject()->AddComponent<HMUI::Touchable*>();

    list = BeatSaberUI::CreateScrollableCustomSourceList<CustomListSource*>(get_transform(), {-50, -40}, {15, 70}, [this](int cellIdx) {
        playlistSelected(cellIdx);
    });
    list->setType(csTypeOf(CoverTableCell*));
    list->tableView->LazyInit();

    RefreshPlaylists();

    auto toggle = BeatSaberUI::CreateToggle(get_transform(), "Download To Playlist", downloadToPlaylistEnabled, {-50, -75}, [this](bool enabled) {
        downloadToPlaylistEnabled = enabled;
        if(enabled)
            SongDownloaderAddon::SelectedPlaylist = selectedPlaylist;
        else
            SongDownloaderAddon::SelectedPlaylist = nullptr;
    });
    toggle->get_transform()->GetParent()->GetComponent<UnityEngine::UI::LayoutElement*>()->set_preferredWidth(45);
    auto sizeFitter = toggle->get_transform()->GetParent()->get_gameObject()->AddComponent<UnityEngine::UI::ContentSizeFitter*>();
    sizeFitter->set_horizontalFit(UnityEngine::UI::ContentSizeFitter::FitMode::PreferredSize);
}

void SongDownloaderAddon::RefreshPlaylists() {
    if(!list)
        return;
    loadedPlaylists = GetLoadedPlaylists();
    std::vector<UnityEngine::Sprite*> newCovers;
    std::vector<std::string> newHovers;
    for(auto& playlist : loadedPlaylists) {
        newCovers.emplace_back(GetCoverImage(playlist));
        newHovers.emplace_back(playlist->name);
    }
    list->replaceSprites(newCovers);
    list->replaceTexts(newHovers);
    list->tableView->ReloadDataKeepingPosition();
}

SongDownloaderAddon* SongDownloaderAddon::Create() {
    return BeatSaberUI::CreateViewController<SongDownloaderAddon*>();
}
