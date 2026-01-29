#ifndef LIBRETRO_OPTIONS_H__
#define LIBRETRO_OPTIONS_H__

#include "libretro.h"

struct retro_core_option_v2_category categories[] = {
    {"video", "Video Settings", "Configure video output."},
    {"system", "System Settings", "Configure emulated hardware."},
    {NULL, NULL, NULL}
};

struct retro_core_option_v2_definition option_defs[] = {
    {
        "cytrus_layout",
        "Screen Layout",
        "layout",
        "Select the screen layout for dual screens.",
        NULL,
        "video",
        {
            {"vertical", "Vertical (Top/Bottom)"},
            {"side", "Side-by-Side"},
            {"single_top", "Single Screen (Top)"},
            {"single_bottom", "Single Screen (Bottom)"},
            {NULL, NULL}
        },
        "vertical"
    },
    {
        "cytrus_resolution_factor",
        "Resolution Factor",
        "resolution",
        "Internal resolution multiplier.",
        NULL,
        "video",
        {
            {"1", "1x (Native)"},
            {"2", "2x"},
            {"3", "3x"},
            {"4", "4x"},
            {NULL, NULL}
        },
        "1"
    },
    {
        "cytrus_is_new_3ds",
        "Emulate New 3DS",
        "new_3ds",
        "Whether to emulate the New 3DS hardware.",
        NULL,
        "system",
        {
            {"true", "Enabled"},
            {"false", "Disabled"},
            {NULL, NULL}
        },
        "false"
    },
    {NULL, NULL, NULL, NULL, NULL, NULL, {{NULL, NULL}}, NULL}
};

struct retro_core_options_v2 core_options_v2 = {
    categories,
    option_defs
};

#endif
