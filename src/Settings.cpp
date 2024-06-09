#include "Settings.hpp"

#include "CustomTypes/CoverTableCell.hpp"
#include "CustomTypes/CustomListSource.hpp"
#include "GlobalNamespace/MenuTransitionsHelper.hpp"
#include "HMUI/ScrollView.hpp"
#include "HMUI/TableView.hpp"
#include "HMUI/Touchable.hpp"
#include "Main.hpp"
#include "PlaylistCore.hpp"
#include "ResettableStaticPtr.hpp"
#include "Types/Config.hpp"
#include "Types/Scroller.hpp"
#include "UnityEngine/Resources.hpp"
#include "bsml/shared/BSML-Lite.hpp"

DEFINE_TYPE(PlaylistCore, SettingsViewController);

using namespace PlaylistCore;

void FixButton(UnityEngine::UI::Button* button, UnityEngine::Vector2 size) {
    if (auto layout = button->GetComponent<UnityEngine::UI::LayoutElement*>()) {
        layout->set_preferredWidth(size.x);
        layout->set_preferredHeight(size.y);
    }
}

void SettingsViewController::DidActivate(bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling) {

    if (!firstActivation)
        return;

    get_gameObject()->AddComponent<HMUI::Touchable*>();

    auto container = BSML::Lite::CreateScrollableSettingsContainer(this);
    auto parent = container->get_transform();

    auto reloadNewButton = BSML::Lite::CreateUIButton(parent, "Reload New Playlists", [] { ReloadPlaylists(false); });
    FixButton(reloadNewButton, {35, 9});
    BSML::Lite::AddHoverHint(reloadNewButton, "Reloads new playlists from the playlist folder");

    auto reloadAllButton = BSML::Lite::CreateUIButton(parent, "Reload All Playlists", [] { ReloadPlaylists(true); });
    FixButton(reloadAllButton, {35, 9});
    BSML::Lite::AddHoverHint(reloadAllButton, "Reloads all playlists from the playlist folder");

    AddConfigValueSlider(parent, getConfig().ScrollSpeed, 1, 0.1, 0.1, 10);
}
