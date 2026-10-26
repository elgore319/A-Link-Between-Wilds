#include "game_version.hpp"

#include "lib.hpp"
#include "lib/util/sys/modules.hpp"
#include "program/loggers.hpp"
#include "program/game/build_id.hpp"

namespace albw::game {

    namespace {
        GameVersion s_version = GameVersion::Unknown;
    }

    GameVersion DetectGameVersion() {
        const auto& rodata = exl::util::GetMainModuleInfo().m_Rodata;
        uint8_t id[BuildIdSize] = {};
        /* The note sits near the start of .rodata, so this returns quickly. */
        if (!FindGnuBuildId(reinterpret_cast<const uint8_t*>(rodata.m_Start), rodata.m_Size, id)) {
            Logging.Log("[albw] game build id not found; game memory will not be touched");
            s_version = GameVersion::Unknown;
            return s_version;
        }

        char hex[33];
        FormatBuildId(id, hex);
        if (SameBuildId(id, Botw100BuildId)) {
            s_version = GameVersion::V1_0_0;
            Logging.Log("[albw] game version 1.0.0 (build id %s): supported", hex);
        } else if (SameBuildId(id, Botw160BuildId)) {
            s_version = GameVersion::V1_6_0;
            Logging.Log("[albw] game version 1.6.0 (build id %s): NOT supported, remove the update in Ryujinx (right-click BotW > Manage Title Updates > No Update)", hex);
        } else {
            s_version = GameVersion::Unknown;
            Logging.Log("[albw] game build id %s is not 1.0.0: NOT supported, game memory will not be touched", hex);
        }
        return s_version;
    }

    GameVersion GetGameVersion() { return s_version; }
}
