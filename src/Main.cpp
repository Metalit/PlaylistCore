#include "Main.hpp"
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

#include "songcore/shared/SongCore.hpp"

#include "bsml/shared/BSML.hpp"

#include "custom-types/shared/delegate.hpp"

#include "GlobalNamespace/StandardLevelDetailViewController.hpp"
#include "GlobalNamespace/LevelCollectionViewController.hpp"
#include "GlobalNamespace/LevelCollectionTableView.hpp"
#include "GlobalNamespace/LevelCollectionNavigationController.hpp"
#include "GlobalNamespace/LevelPackDetailViewController.hpp"
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
#include "GlobalNamespace/BeatmapLevel.hpp"
#include "GlobalNamespace/IEntitlementModel.hpp"

#include "UnityEngine/Resources.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Rect.hpp" // This needs to be included before RectTransform
#include "UnityEngine/RectTransform.hpp"
#include "UnityEngine/Events/UnityAction.hpp"
#include "UnityEngine/UI/Button.hpp"
#include "UnityEngine/UI/VerticalLayoutGroup.hpp"
#include "HMUI/TableView.hpp"
#include "HMUI/ScrollView.hpp"
#include "HMUI/ViewController.hpp"
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

modloader::ModInfo modInfo = {"PlaylistCore", VERSION, 0};
modloader::ModInfo managerModInfo = {"PlaylistManager", VERSION, 0};

bool hasManager;

std::string GetPlaylistsPath() {
    static std::string playlistsPath(getDataDir(managerModInfo) + "Playlists");
    return playlistsPath;
}

std::string GetCoversPath() {
    static std::string coversPath(getDataDir(managerModInfo) + "Covers");
    return coversPath;
}

// override header cell behavior and change no data prefab
MAKE_HOOK_MATCH(LevelCollectionViewController_SetData, &LevelCollectionViewController::SetData,
        void, LevelCollectionViewController* self, ArrayW<BeatmapLevel*> beatmapLevels, StringW headerText, UnityEngine::Sprite* headerSprite, bool sortLevels, bool sortPreviewBeatmapLevels, UnityEngine::GameObject* noDataInfoPrefab) {
    // only check for null strings, not empty
    // string will be null for level collections but not level packs
    self->_showHeader = (bool) headerText;
    // copy base game method implementation
    self->_levelCollectionTableView->Init(headerText, headerSprite);
    if(self->_noDataInfoGO) {
        UnityEngine::Object::Destroy(self->_noDataInfoGO);
        self->_noDataInfoGO = nullptr;
    }
    // also override check for empty collection
    if(beatmapLevels ) {
        self->_levelCollectionTableView->get_gameObject()->SetActive(true);
        self->_levelCollectionTableView->SetData(reinterpret_cast<System::Collections::Generic::IReadOnlyList_1<BeatmapLevel*>*>(beatmapLevels.convert()), self->_playerDataModel->playerData->favoritesLevelIds, sortLevels, sortPreviewBeatmapLevels);
        self->_levelCollectionTableView->RefreshLevelsAvailability();
    } else {
        self->_levelCollectionTableView->SetData(ListW<BeatmapLevel*>::New()->i___System__Collections__Generic__IReadOnlyList_1_T_(), self->_playerDataModel->playerData->favoritesLevelIds, sortLevels, sortPreviewBeatmapLevels);
        if(noDataInfoPrefab)
            self->_noDataInfoGO = self->_container->InstantiatePrefab(noDataInfoPrefab, self->_noDataInfoContainer);
        // change no custom songs text if playlists exist
        // because if they do then the only way to get here with that specific no data indicator is to have no playlists filtered
        static ConstString message("No playlists are contained in the filtering options selected.");
        if(GetLoadedPlaylists().size() > 0 && !SongCore::API::Loading::GetAllLevels().empty() && noDataInfoPrefab == FindComponent<LevelFilteringNavigationController*>()->_emptyCustomSongListInfoPrefab.ptr())
            self->_noDataInfoGO->GetComponentInChildren<TMPro::TextMeshProUGUI*>()->set_text(message);
        self->_levelCollectionTableView->get_gameObject()->SetActive(false);
    }
    if(self->get_isInViewControllerHierarchy()) {
        if(self->_showHeader)
            self->_levelCollectionTableView->SelectLevelPackHeaderCell();
        else
            self->_levelCollectionTableView->ClearSelection();
        self->_songPreviewPlayer->CrossfadeToDefault();
    }
}

// make playlist selector only 5 playlists wide and add scrolling
MAKE_HOOK_MATCH(AnnotatedBeatmapLevelCollectionsGridView_OnEnable, &AnnotatedBeatmapLevelCollectionsGridView::OnEnable,
        void, AnnotatedBeatmapLevelCollectionsGridView* self) {

    self->GetComponent<UnityEngine::RectTransform*>()->set_anchorMax({0.83, 1});
    self->_pageControl->_content->get_gameObject()->SetActive(false);
    auto content = self->_animator->_contentTransform;
    content->set_anchoredPosition({0, content->get_anchoredPosition().y});

    AnnotatedBeatmapLevelCollectionsGridView_OnEnable(self);

    if(!self->GetComponent<Scroller*>())
        self->get_gameObject()->AddComponent<Scroller*>()->Init(self->_animator->_contentTransform);
}

// make the playlist opening animation work better with the playlist scroller
MAKE_HOOK_MATCH(AnnotatedBeatmapLevelCollectionsGridViewAnimator_AnimateOpen, &AnnotatedBeatmapLevelCollectionsGridViewAnimator::AnimateOpen,
        void, AnnotatedBeatmapLevelCollectionsGridViewAnimator* self, bool animated) {

    // store actual values to avoid breaking things when closing
    int rowCount = self->_rowCount;
    int selectedRow = self->_selectedRow;

    // lock height to specific value
    self->_rowCount = 5;
    self->_selectedRow = 0;

    AnnotatedBeatmapLevelCollectionsGridViewAnimator_AnimateOpen(self, animated);

    // prevent modification of content transform as it overrides the scroll view
    Tweening::Vector2Tween::getStaticF_Pool()->Despawn(self->_contentPositionTween);
    self->_contentPositionTween = nullptr;

    self->_rowCount = rowCount;
    self->_selectedRow = selectedRow;
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
        void, AnnotatedBeatmapLevelCollectionCell* self, IEntitlementModel* entitlementModel) {

    AnnotatedBeatmapLevelCollectionCell_RefreshAvailabilityAsync(self, entitlementModel);

    if(hasManager)
        return;

    auto pack = self->_beatmapLevelPack;
    if(pack) {
        auto playlist = GetPlaylistWithPrefix(pack->packID);
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

// override to prevent crashes due to opening with a null level pack
#define COMBINE(delegate1, selfMethodName, ...) delegate1 = (__VA_ARGS__) System::Delegate::Combine(delegate1, System::Delegate::CreateDelegate(csTypeOf(__VA_ARGS__), self, #selfMethodName));
MAKE_HOOK_MATCH(LevelCollectionNavigationController_DidActivate, &LevelCollectionNavigationController::DidActivate,
        void, LevelCollectionNavigationController* self, bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling) {

    if(addedToHierarchy) {
        COMBINE(self->_levelCollectionViewController->didSelectLevelEvent, HandleLevelCollectionViewControllerDidSelectLevel, System::Action_2<UnityW<LevelCollectionViewController>, BeatmapLevel*>*);
        COMBINE(self->_levelCollectionViewController->didSelectHeaderEvent, HandleLevelCollectionViewControllerDidSelectPack, System::Action_1<UnityW<LevelCollectionViewController>>*);
        COMBINE(self->_levelDetailViewController->didPressActionButtonEvent, HandleLevelDetailViewControllerDidPressActionButton, System::Action_1<UnityW<StandardLevelDetailViewController>>*);
        COMBINE(self->_levelDetailViewController->didPressPracticeButtonEvent, HandleLevelDetailViewControllerDidPressPracticeButton, System::Action_2<UnityW<StandardLevelDetailViewController>, BeatmapLevel*>*);
        COMBINE(self->_levelDetailViewController->didChangeDifficultyBeatmapEvent, HandleLevelDetailViewControllerDidChangeDifficultyBeatmap, System::Action_1<UnityW<StandardLevelDetailViewController>>*);
        COMBINE(self->_levelDetailViewController->didChangeContentEvent, HandleLevelDetailViewControllerDidPresentContent, System::Action_2<UnityW<StandardLevelDetailViewController>, StandardLevelDetailViewController::ContentType>*);
        COMBINE(self->_levelDetailViewController->didPressOpenLevelPackButtonEvent, HandleLevelDetailViewControllerDidPressOpenLevelPackButton, System::Action_2<UnityW<StandardLevelDetailViewController>, BeatmapLevelPack*>*);
        COMBINE(self->_levelDetailViewController->levelFavoriteStatusDidChangeEvent, HandleLevelDetailViewControllerLevelFavoriteStatusDidChange, System::Action_2<UnityW<StandardLevelDetailViewController>, bool>*);
        if (self->_beatmapLevelToBeSelectedAfterPresent) {
            self->_levelCollectionViewController->SelectLevel(self->_beatmapLevelToBeSelectedAfterPresent);
            self->SetChildViewController(self->_levelCollectionViewController);
            self->_beatmapLevelToBeSelectedAfterPresent = nullptr;
        }
        else {
            // override here so that the pack detail controller will not be shown if no pack is selected
            if (self->_levelPack) {
                ArrayW<HMUI::ViewController*> children{2};
                children[0] = self->_levelCollectionViewController;
                children[1] = self->_levelPackDetailViewController;
                self->SetChildViewControllers(children);
            } else
                self->SetChildViewController(self->_levelCollectionViewController);
        }
    } else if(self->_loading) {
        self->ClearChildViewControllers();
    }
    else if(self->_hideDetailViewController) {
        self->SetChildViewController(self->_levelCollectionViewController);
        self->_hideDetailViewController = false;
    }
}

extern "C" void setup(CModInfo* info) {
    info->id = "PlaylistCore";
    info->version = VERSION;
    info->version_long = 0;

    auto playlistsPath = GetPlaylistsPath();
    if(!direxists(playlistsPath))
        mkpath(playlistsPath);

    LOG_INFO("Playlist path is {}", playlistsPath);

    auto coversPath = GetCoversPath();
    if(!direxists(coversPath))
        mkpath(coversPath);

    getConfig().Init(modInfo);
}

extern "C" void late_load() {
    LOG_INFO("Starting PlaylistCore installation...");
    il2cpp_functions::Init();
    custom_types::Register::AutoRegister();

    BSML::Init();
    BSML::Register::RegisterMenuButton("Reload Playlists", "Reloads all playlists!", []{ SongCore::API::Loading::RefreshLevelPacks(); });
    BSML::Register::RegisterSettingsMenu<SettingsViewController*>("Playlist Core");

    auto managerCInfo = managerModInfo.to_c();
    hasManager = modloader_require_mod(&managerCInfo, CMatchType::MatchType_IdOnly);

    INSTALL_HOOK_ORIG(logger, LevelCollectionViewController_SetData);
    INSTALL_HOOK(logger, AnnotatedBeatmapLevelCollectionsGridView_OnEnable);
    INSTALL_HOOK(logger, AnnotatedBeatmapLevelCollectionsGridViewAnimator_AnimateOpen);
    INSTALL_HOOK(logger, AnnotatedBeatmapLevelCollectionsGridViewAnimator_ScrollToRowIdxInstant);
    INSTALL_HOOK(logger, AnnotatedBeatmapLevelCollectionCell_RefreshAvailabilityAsync);
    INSTALL_HOOK(logger, MenuTransitionsHelper_RestartGame);
    INSTALL_HOOK_ORIG(logger, LevelCollectionNavigationController_DidActivate);

    SongCore::API::Loading::GetCustomLevelPacksWillRefreshEvent().addCallback(
        [](SongCore::SongLoader::CustomBeatmapLevelsRepository* customBeatmapLevelsRepository) {
            LoadPlaylists(customBeatmapLevelsRepository, true);
        }
    );

    LOG_INFO("Successfully installed PlaylistCore!");
}
