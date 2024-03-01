#pragma once

#include "HMUI/ViewController.hpp"

#include "custom-types/shared/macros.hpp"

DECLARE_CLASS_CODEGEN(PlaylistCore, SettingsViewController, HMUI::ViewController,

    DECLARE_OVERRIDE_METHOD(void, DidActivate, il2cpp_utils::il2cpp_type_check::MetadataGetter<&HMUI::ViewController::DidActivate>::methodInfo(), bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling);
)