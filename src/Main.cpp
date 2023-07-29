#include "Main.hpp"
#include "Types/SongDownloaderAddon.hpp"
#include "Types/Scroller.hpp"
#include "Types/Config.hpp"
#include "PlaylistCore.hpp"
#include "Settings.hpp"
#include "Utils.hpp"
#include "ResettableStaticPtr.hpp"

#include <chrono>

#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"
#include "beatsaber-hook/shared/config/config-utils.hpp"
#include "beatsaber-hook/shared/utils/hooking.hpp"

#include "songloader/shared/API.hpp"

#include "questui/shared/QuestUI.hpp"
#include "bsml/shared/BSML.hpp"

#include "custom-types/shared/delegate.hpp"

#include "GlobalNamespace/StandardLevelDetailViewController.hpp"
#include "GlobalNamespace/LevelCollectionViewController.hpp"
#include "GlobalNamespace/LevelCollectionTableView.hpp"
#include "GlobalNamespace/LevelCollectionNavigationController.hpp"
#include "GlobalNamespace/LevelPackDetailViewController.hpp"
#include "GlobalNamespace/LevelPackDetailViewController_ContentType.hpp"
#include "GlobalNamespace/MenuTransitionsHelper.hpp"
#include "GlobalNamespace/BeatmapDifficultySegmentedControlController.hpp"
#include "GlobalNamespace/AnnotatedBeatmapLevelCollectionsViewController.hpp"
#include "GlobalNamespace/AnnotatedBeatmapLevelCollectionsGridView.hpp"
#include "GlobalNamespace/AnnotatedBeatmapLevelCollectionsGridViewAnimator.hpp"
#include "GlobalNamespace/AnnotatedBeatmapLevelCollectionCell.hpp"
#include "GlobalNamespace/PageControl.hpp"
#include "GlobalNamespace/LevelFilteringNavigationController.hpp"
#include "GlobalNamespace/PlayerData.hpp"
#include "GlobalNamespace/PlayerDataModel.hpp"
#include "GlobalNamespace/SongPreviewPlayer.hpp"
#include "GlobalNamespace/StandardLevelInfoSaveData.hpp"
#include "GlobalNamespace/ISpriteAsyncLoader.hpp"
#include "GlobalNamespace/EnvironmentInfoSO.hpp"
#include "GlobalNamespace/PreviewDifficultyBeatmapSet.hpp"
#include "GlobalNamespace/IDifficultyBeatmap.hpp"
#include "GlobalNamespace/IBeatmapLevel.hpp"

#include "UnityEngine/Resources.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Rect.hpp" // This needs to be included before RectTransform
#include "UnityEngine/RectTransform.hpp"
#include "UnityEngine/Events/UnityAction.hpp"
#include "UnityEngine/UI/Button_ButtonClickedEvent.hpp"
#include "UnityEngine/UI/VerticalLayoutGroup.hpp"
#include "HMUI/TableView.hpp"
#include "HMUI/ScrollView.hpp"
#include "HMUI/ViewController_AnimationType.hpp"
#include "HMUI/FlowCoordinator.hpp"
#include "HMUI/InputFieldView.hpp"
#include "Tweening/TimeTweeningManager.hpp"
#include "Tweening/Vector2Tween.hpp"
#include "Zenject/DiContainer.hpp"
#include "Zenject/StaticMemoryPool_7.hpp"
#include "System/Tuple_2.hpp"
#include "System/Action_1.hpp"
#include "System/Action_2.hpp"
#include "System/Collections/Generic/HashSet_1.hpp"

using namespace GlobalNamespace;
using namespace PlaylistCore;

ModInfo modInfo;
ModInfo managerModInfo;

bool hasManager;

Logger& getLogger() {
    static auto logger = new Logger(modInfo, LoggerOptions(false, true));
    return *logger;
}

std::string GetPlaylistsPath() {
    static std::string playlistsPath(getDataDir(managerModInfo) + "Playlists");
    return playlistsPath;
}

std::string GetBackupsPath() {
    static std::string backupsPath(getDataDir(managerModInfo) + "PlaylistBackups");
    return backupsPath;
}

std::string GetCoversPath() {
    static std::string coversPath(getDataDir(managerModInfo) + "Covers");
    return coversPath;
}

// small fix for horizontal tables
MAKE_HOOK_MATCH(TableView_ReloadDataKeepingPosition, &HMUI::TableView::ReloadDataKeepingPosition,
        void, HMUI::TableView* self) {

    self->ReloadData();

    auto rect = self->viewportTransform->get_rect();
    float viewSize = (self->tableType == HMUI::TableView::TableType::Vertical) ? rect.get_height() : rect.get_width();
    float position = self->scrollView->get_position();

    self->scrollView->ScrollTo(std::min(position, std::max(self->cellSize * self->numberOfCells - viewSize, 0.0f)), false);
}

// override header cell behavior and change no data prefab
MAKE_HOOK_MATCH(LevelCollectionViewController_SetData, &LevelCollectionViewController::SetData,
        void, LevelCollectionViewController* self, IBeatmapLevelCollection* beatmapLevelCollection, StringW headerText, UnityEngine::Sprite* headerSprite, bool sortLevels, UnityEngine::GameObject* noDataInfoPrefab) {
    
    // only check for null strings, not empty
    // string will be null for level collections but not level packs
    self->showHeader = (bool) headerText;
    // copy base game method implementation
    self->levelCollectionTableView->Init(headerText, headerSprite);
    self->levelCollectionTableView->showLevelPackHeader = self->showHeader;
    if(self->noDataInfoGO) {
        UnityEngine::Object::Destroy(self->noDataInfoGO);
        self->noDataInfoGO = nullptr;
    }
    // also override check for empty collection
    if(beatmapLevelCollection) {
        self->levelCollectionTableView->get_gameObject()->SetActive(true);
        self->levelCollectionTableView->SetData(beatmapLevelCollection->get_beatmapLevels(), self->playerDataModel->playerData->favoritesLevelIds, sortLevels);
        self->levelCollectionTableView->RefreshLevelsAvailability();
    } else {
        if(noDataInfoPrefab)
            self->noDataInfoGO = self->container->InstantiatePrefab(noDataInfoPrefab, self->noDataInfoContainer);
        // change no custom songs text if playlists exist
        // because if they do then the only way to get here with that specific no data indicator is to have no playlists filtered
        static ConstString message("No playlists are contained in the filtering options selected.");
        if(GetLoadedPlaylists().size() > 0 && noDataInfoPrefab == FindComponent<LevelFilteringNavigationController*>()->emptyCustomSongListInfoPrefab)
            self->noDataInfoGO->GetComponentInChildren<TMPro::TextMeshProUGUI*>()->set_text(message);
        self->levelCollectionTableView->get_gameObject()->SetActive(false);
    }
    if(self->get_isInViewControllerHierarchy()) {
        if(self->showHeader)
            self->levelCollectionTableView->SelectLevelPackHeaderCell();
        else
            self->levelCollectionTableView->ClearSelection();
        self->songPreviewPlayer->CrossfadeToDefault();
    }
}

// make playlist selector only 5 playlists wide and add scrolling
MAKE_HOOK_MATCH(AnnotatedBeatmapLevelCollectionsGridView_OnEnable, &AnnotatedBeatmapLevelCollectionsGridView::OnEnable,
        void, AnnotatedBeatmapLevelCollectionsGridView* self) {

    self->GetComponent<UnityEngine::RectTransform*>()->set_anchorMax({0.83, 1});
    self->pageControl->content->get_gameObject()->SetActive(false);
    auto content = self->animator->contentTransform;
    content->set_anchoredPosition({0, content->get_anchoredPosition().y});
    
    AnnotatedBeatmapLevelCollectionsGridView_OnEnable(self);

    if(!self->GetComponent<Scroller*>())
        self->get_gameObject()->AddComponent<Scroller*>()->Init(self->animator->contentTransform);
}

// make the playlist opening animation work better with the playlist scroller
MAKE_HOOK_MATCH(AnnotatedBeatmapLevelCollectionsGridViewAnimator_AnimateOpen, &AnnotatedBeatmapLevelCollectionsGridViewAnimator::AnimateOpen,
        void, AnnotatedBeatmapLevelCollectionsGridViewAnimator* self, bool animated) {
    
    // store actual values to avoid breaking things when closing
    int rowCount = self->rowCount;
    int selectedRow = self->selectedRow;
    
    // lock height to specific value
    self->rowCount = 5;
    self->selectedRow = 0;

    AnnotatedBeatmapLevelCollectionsGridViewAnimator_AnimateOpen(self, animated);
    
    // prevent modification of content transform as it overrides the scroll view
    Tweening::Vector2Tween::_get_Pool()->Despawn(self->contentPositionTween);
    self->contentPositionTween = nullptr;
    
    self->rowCount = rowCount;
    self->selectedRow = selectedRow;
}

// ensure animator doesn't get stuck at the wrong position
MAKE_HOOK_MATCH(AnnotatedBeatmapLevelCollectionsGridViewAnimator_ScrollToRowIdxInstant, &AnnotatedBeatmapLevelCollectionsGridViewAnimator::ScrollToRowIdxInstant,
        void, AnnotatedBeatmapLevelCollectionsGridViewAnimator* self, int selectedRow) {

    // despawns tweens and force sets the viewport and anchored pos
    self->AnimateClose(selectedRow, false);

    AnnotatedBeatmapLevelCollectionsGridViewAnimator_ScrollToRowIdxInstant(self, selectedRow);
}

// prevent download icon showing up on empty custom playlists unless manager is changing the behavior
MAKE_HOOK_MATCH(AnnotatedBeatmapLevelCollectionCell_RefreshAvailabilityAsync, &AnnotatedBeatmapLevelCollectionCell::RefreshAvailabilityAsync,
        void, AnnotatedBeatmapLevelCollectionCell* self, AdditionalContentModel* contentModel) {
    
    AnnotatedBeatmapLevelCollectionCell_RefreshAvailabilityAsync(self, contentModel);

    if(hasManager)
        return;

    auto pack = il2cpp_utils::try_cast<IBeatmapLevelPack>(self->annotatedBeatmapLevelCollection);
    if(pack.has_value()) {
        auto playlist = GetPlaylistWithPrefix(pack.value()->get_packID());
        if(playlist)
            self->SetDownloadIconVisible(false);
    }
}

// throw away objects on a soft restart
MAKE_HOOK_MATCH(MenuTransitionsHelper_RestartGame, &MenuTransitionsHelper::RestartGame,
        void, MenuTransitionsHelper* self, System::Action_1<Zenject::DiContainer*>* finishCallback) {

    for(auto scroller : UnityEngine::Resources::FindObjectsOfTypeAll<Scroller*>()) {
        UnityEngine::Object::Destroy(scroller);
    }

    ClearLoadedImages();

    hasLoaded = false;

    MenuTransitionsHelper_RestartGame(self, finishCallback);
    
    ResettableStaticPtr::resetAll();
}

// add our playlist selection view controller to the song downloader menu
MAKE_HOOK_FIND_CLASS_INSTANCE(DownloadSongsFlowCoordinator_DidActivate, "SongDownloader", "DownloadSongsFlowCoordinator", "DidActivate",
        void, HMUI::FlowCoordinator* self, bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling) {
    
    DownloadSongsFlowCoordinator_DidActivate(self, firstActivation, addedToHierarchy, screenSystemEnabling);

    if(firstActivation) {
        STATIC_AUTO(AddonViewController, SongDownloaderAddon::Create());
        self->providedRightScreenViewController = AddonViewController;
    }
}

// add a playlist callback to the song downloader buttons
MAKE_HOOK_FIND_CLASS_INSTANCE(DownloadSongsSearchViewController_DidActivate, "SongDownloader", "DownloadSongsSearchViewController", "DidActivate",
        void, HMUI::ViewController* self, bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling) {
    
    DownloadSongsSearchViewController_DidActivate(self, firstActivation, addedToHierarchy, screenSystemEnabling);

    if(!firstActivation)
        return;

    using namespace UnityEngine;
    
    // add listeners to search entry buttons
    auto entryList = self->GetComponentsInChildren<UI::VerticalLayoutGroup*>().First([](UI::VerticalLayoutGroup* x) {
        return x->get_name() == "QuestUIScrollViewContentContainer";
    })->get_transform();

    // offset by one because the prefab doesn't seem to be destroyed yet here
    int entryCount = entryList->GetChildCount() - 1;
    for(int i = 0; i < entryCount; i++) {
        // undo offset to skip first element
        auto downloadButton = entryList->GetChild(i + 1)->GetComponentInChildren<UI::Button*>();
        // get entry at index with some lovely pointer addition
        SearchEntryHack* entryArrStart = (SearchEntryHack*) (((char*) self) + sizeof(HMUI::ViewController));
        // capture button array start and index
        downloadButton->get_onClick()->AddListener(custom_types::MakeDelegate<Events::UnityAction*>((std::function<void()>) [entryArrStart, i] {
            auto& entry = *(entryArrStart + i + 1);
            // get hash from entry
            std::string hash;
            if(entry.MapType == SearchEntryHack::MapType::BeatSaver)
                hash = entry.map.GetVersions().front().GetHash();
            else if(entry.MapType == SearchEntryHack::MapType::BeastSaber)
                hash = entry.BSsong.GetHash();
            else if(entry.MapType == SearchEntryHack::MapType::ScoreSaber)
                hash = entry.SSsong.GetId();
            // get json object from playlist
            auto playlist = SongDownloaderAddon::SelectedPlaylist;
            if(!playlist)
                return;
            auto& json = playlist->playlistJSON;
            // add a blank song
            json.Songs.emplace_back(BPSong());
            // set info
            auto& songJson = *(json.Songs.end() - 1);
            songJson.Hash = hash;
            // write to file
            playlist->Save();
            // have changes be updated
            MarkPlaylistForReload(playlist);
        }));
    }
}

// override to prevent crashes due to opening with a null level pack
#define COMBINE(delegate1, selfMethodName, ...) delegate1 = (__VA_ARGS__) System::Delegate::Combine(delegate1, System::Delegate::CreateDelegate(csTypeOf(__VA_ARGS__), self, #selfMethodName));
MAKE_HOOK_MATCH(LevelCollectionNavigationController_DidActivate, &LevelCollectionNavigationController::DidActivate,
        void, LevelCollectionNavigationController* self, bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling) {

    if(addedToHierarchy) {
        COMBINE(self->levelCollectionViewController->didSelectLevelEvent, HandleLevelCollectionViewControllerDidSelectLevel, System::Action_2<LevelCollectionViewController*, IPreviewBeatmapLevel*>*);
        COMBINE(self->levelCollectionViewController->didSelectHeaderEvent, HandleLevelCollectionViewControllerDidSelectPack, System::Action_1<LevelCollectionViewController*>*);
        COMBINE(self->levelDetailViewController->didPressActionButtonEvent, HandleLevelDetailViewControllerDidPressActionButton, System::Action_1<StandardLevelDetailViewController*>*);
        COMBINE(self->levelDetailViewController->didPressPracticeButtonEvent, HandleLevelDetailViewControllerDidPressPracticeButton, System::Action_2<StandardLevelDetailViewController*, IBeatmapLevel*>*);
        COMBINE(self->levelDetailViewController->didChangeDifficultyBeatmapEvent, HandleLevelDetailViewControllerDidChangeDifficultyBeatmap, System::Action_2<StandardLevelDetailViewController*, IDifficultyBeatmap*>*);
        COMBINE(self->levelDetailViewController->didChangeContentEvent, HandleLevelDetailViewControllerDidPresentContent, System::Action_2<StandardLevelDetailViewController*, StandardLevelDetailViewController::ContentType>*);
        COMBINE(self->levelDetailViewController->didPressOpenLevelPackButtonEvent, HandleLevelDetailViewControllerDidPressOpenLevelPackButton, System::Action_2<StandardLevelDetailViewController*, IBeatmapLevelPack*>*);
        COMBINE(self->levelDetailViewController->levelFavoriteStatusDidChangeEvent, HandleLevelDetailViewControllerLevelFavoriteStatusDidChange, System::Action_2<StandardLevelDetailViewController*, bool>*);
        if (self->beatmapLevelToBeSelectedAfterPresent) {
            self->levelCollectionViewController->SelectLevel(self->beatmapLevelToBeSelectedAfterPresent);
            self->SetChildViewController(self->levelCollectionViewController);
            self->beatmapLevelToBeSelectedAfterPresent = nullptr;
        }
        else {
            // override here so that the pack detail controller will not be shown if no pack is selected
            if (self->levelPack) {
                ArrayW<HMUI::ViewController*> children{2};
                children[0] = self->levelCollectionViewController;
                children[1] = self->levelPackDetailViewController;
                self->SetChildViewControllers(children);
            } else
                self->SetChildViewController(self->levelCollectionViewController);
        }
    } else if(self->loading) {
        self->ClearChildViewControllers();
    }
    else if(self->hideDetailViewController) {
        self->PresentViewControllersForLevelCollection();
        self->hideDetailViewController = false;
    }
}

extern "C" void setup(ModInfo& info) {
    modInfo.id = "PlaylistCore";
    modInfo.version = VERSION;
    info = modInfo;
    managerModInfo.id = "PlaylistManager";
    managerModInfo.version = VERSION;
    
    auto playlistsPath = GetPlaylistsPath();
    if(!direxists(playlistsPath))
        mkpath(playlistsPath);
    
    LOG_INFO("%s", playlistsPath.c_str());
    
    auto backupsPath = GetBackupsPath();
    if(!direxists(backupsPath))
        mkpath(backupsPath);
    
    auto coversPath = GetCoversPath();
    if(!direxists(coversPath))
        mkpath(coversPath);

    getConfig().Init(modInfo);
}

extern "C" void load() {
    LOG_INFO("Starting PlaylistCore installation...");
    il2cpp_functions::Init();
    QuestUI::Init();
    QuestUI::Register::RegisterModSettingsViewController<SettingsViewController*>(modInfo, "Playlist Core");
    
    BSML::Init();
    BSML::Register::RegisterMenuButton("Reload Playlists", "Reloads all playlists!", []{ RuntimeSongLoader::API::RefreshSongs(false); });
    hasManager = Modloader::requireMod("PlaylistManager");

    INSTALL_HOOK_ORIG(getLogger(), TableView_ReloadDataKeepingPosition);
    INSTALL_HOOK_ORIG(getLogger(), LevelCollectionViewController_SetData);
    INSTALL_HOOK(getLogger(), AnnotatedBeatmapLevelCollectionsGridView_OnEnable);
    INSTALL_HOOK(getLogger(), AnnotatedBeatmapLevelCollectionsGridViewAnimator_AnimateOpen);
    INSTALL_HOOK(getLogger(), AnnotatedBeatmapLevelCollectionsGridViewAnimator_ScrollToRowIdxInstant);
    INSTALL_HOOK(getLogger(), AnnotatedBeatmapLevelCollectionCell_RefreshAvailabilityAsync);
    INSTALL_HOOK(getLogger(), MenuTransitionsHelper_RestartGame);
    INSTALL_HOOK(getLogger(), DownloadSongsFlowCoordinator_DidActivate);
    INSTALL_HOOK(getLogger(), DownloadSongsSearchViewController_DidActivate);
    INSTALL_HOOK_ORIG(getLogger(), LevelCollectionNavigationController_DidActivate);
    
    RuntimeSongLoader::API::AddRefreshLevelPacksEvent(
        [] (RuntimeSongLoader::SongLoaderBeatmapLevelPackCollectionSO* customBeatmapLevelPackCollectionSO) {
            LoadPlaylists(customBeatmapLevelPackCollectionSO, true);
        }
    );
    
    LOG_INFO("Successfully installed PlaylistCore!");
}
