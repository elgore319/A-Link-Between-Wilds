#pragma once

#include "program/config/config_parser.hpp"

namespace albw::config {

    /*
     * Fills the active settings from the compiled-in defaults (albw_config.hpp),
     * then overrides them from sd:/albw/config.ini if that file exists.
     *
     * Call once, from the network thread, before the first Get(). Never fails:
     * a missing or broken file just means the defaults are used, and every
     * problem is logged with an [albw] prefix.
     */
    void Load();

    /* The active settings. Valid after Load(). */
    const Settings& Get();
}
