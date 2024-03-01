#include "Main.hpp"
#include "Settings.hpp"
#include "Types/Config.hpp"
#include "CustomTypes/CustomListSource.hpp"
#include "CustomTypes/CoverTableCell.hpp"
#include "Types/Scroller.hpp"
#include "PlaylistCore.hpp"
#include "ResettableStaticPtr.hpp"

#include "bsml/shared/BSML-Lite.hpp"

#include "GlobalNamespace/MenuTransitionsHelper.hpp"
#include "HMUI/Touchable.hpp"
#include "HMUI/ScrollView.hpp"
#include "HMUI/TableView.hpp"
#include "UnityEngine/Resources.hpp"

DEFINE_TYPE(PlaylistCore, SettingsViewController);

using namespace PlaylistCore;

void SettingsViewController::DidActivate(bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling) {

    using Vec = UnityEngine::Vector2;

    if(!firstActivation)
        return;

    get_gameObject()->AddComponent<HMUI::Touchable*>();

    auto container = BSML::Lite::CreateScrollableSettingsContainer(this);
    auto parent = container->get_transform();

    auto horizontal = BSML::Lite::CreateHorizontalLayoutGroup(parent);
    horizontal->set_childControlWidth(false);
    horizontal->set_childAlignment(UnityEngine::TextAnchor::MiddleCenter);
    auto reloadNewButton = BSML::Lite::CreateUIButton(horizontal, "Reload New Playlists", Vec{0, 0}, Vec{40, 10}, [] {
        ReloadPlaylists(false);
    });
    BSML::Lite::AddHoverHint(reloadNewButton, "Reloads new playlists from the playlist folder");

    horizontal = BSML::Lite::CreateHorizontalLayoutGroup(parent);
    horizontal->set_childControlWidth(false);
    horizontal->set_childAlignment(UnityEngine::TextAnchor::MiddleCenter);
    auto reloadAllButton = BSML::Lite::CreateUIButton(horizontal, "Reload All Playlists", Vec{0, 0}, Vec{40, 10}, [] {
        ReloadPlaylists(true);
    });
    BSML::Lite::AddHoverHint(reloadAllButton, "Reloads all playlists from the playlist folder");

    AddConfigValueSlider(parent, getConfig().ScrollSpeed, 1, 0.1, 0.1, 10);
}
