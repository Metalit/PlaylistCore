#pragma once

#include "config-utils/shared/config-utils.hpp"

DECLARE_CONFIG(Config,
    private:
    NAMED_VALUE_DEFAULT(float, ScrollSpeedOldName, 2, "scrollSpeed");
    public:
    CONFIG_VALUE(ScrollSpeed, float, "Playlist Scroll Speed", -1, "The speed at which to scroll through the playlists with the joystick.");
    CONFIG_VALUE(Order, std::vector<std::string>, "order", {});
    DESERIALIZE_ACTION(UpdateName,
        if(self->ScrollSpeed.GetValue() == -1)
            self->ScrollSpeed.SetValue(self->ScrollSpeedOldName, false);
    )
)
