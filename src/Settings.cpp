#include "Main.hpp"
#include "Settings.hpp"
#include "Types/Config.hpp"
#include "CustomTypes/CustomListSource.hpp"
#include "CustomTypes/CoverTableCell.hpp"
#include "Types/Scroller.hpp"
#include "PlaylistCore.hpp"
#include "ResettableStaticPtr.hpp"

#include "questui/shared/BeatSaberUI.hpp"

#include "GlobalNamespace/MenuTransitionsHelper.hpp"
#include "HMUI/Touchable.hpp"
#include "HMUI/ScrollView.hpp"
#include "HMUI/TableView_ScrollPositionType.hpp"
#include "UnityEngine/Resources.hpp"

DEFINE_TYPE(PlaylistCore, SettingsViewController);

using namespace QuestUI;
using namespace PlaylistCore;

void SettingsViewController::DidActivate(bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling) {

    using Vec = UnityEngine::Vector2;

    if(!firstActivation)
        return;

    get_gameObject()->AddComponent<HMUI::Touchable*>();

    auto container = BeatSaberUI::CreateScrollableSettingsContainer(this);
    auto parent = container->get_transform();

    auto horizontal = BeatSaberUI::CreateHorizontalLayoutGroup(parent);
    horizontal->set_childControlWidth(false);
    horizontal->set_childAlignment(UnityEngine::TextAnchor::MiddleCenter);
    auto reloadNewButton = BeatSaberUI::CreateUIButton(horizontal, "Reload New Playlists", Vec{0, 0}, Vec{40, 10}, [] {
        ReloadPlaylists(false);
    });
    BeatSaberUI::AddHoverHint(reloadNewButton, "Reloads new playlists from the playlist folder");

    horizontal = BeatSaberUI::CreateHorizontalLayoutGroup(parent);
    horizontal->set_childControlWidth(false);
    horizontal->set_childAlignment(UnityEngine::TextAnchor::MiddleCenter);
    auto reloadAllButton = BeatSaberUI::CreateUIButton(horizontal, "Reload All Playlists", Vec{0, 0}, Vec{40, 10}, [] {
        ReloadPlaylists(true);
    });
    BeatSaberUI::AddHoverHint(reloadAllButton, "Reloads all playlists from the playlist folder");

    AddConfigValueSlider(parent, getConfig().ScrollSpeed, 1, 0.1, 0.1, 10);
}
