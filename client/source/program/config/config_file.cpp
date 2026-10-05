#include "config_file.hpp"

#include "lib.hpp"
#include <nn/fs.hpp>
#include "program/albw_config.hpp"
#include "program/loggers.hpp"

namespace albw::config {

    namespace {
        Settings s_settings;

        constexpr const char MountName[] = "sd";
        constexpr const char ConfigPath[] = "sd:/albw/config.ini";

        /* Way more than a config file should ever need; anything bigger is a mistake. */
        constexpr long MaxFileSize = 4096;
        char s_file_buf[MaxFileSize];

        void CopyStr(char* dst, size_t cap, const char* src) {
            size_t i = 0;
            for (; i + 1 < cap && src[i] != '\0'; i++) dst[i] = src[i];
            for (; i < cap; i++) dst[i] = '\0';
        }

        void SetDefaults() {
            CopyStr(s_settings.server_ip, sizeof(s_settings.server_ip), ServerIp);
            s_settings.server_port = ServerPort;
            CopyStr(s_settings.player_name, sizeof(s_settings.player_name), PlayerName);
        }

        void OnWarning(void*, int line, const char* msg) {
            Logging.Log("[albw] config.ini line %d: %s (ignored)", line, msg);
        }

        /* Returns the number of bytes read into s_file_buf, or -1 if there is no usable file. */
        long ReadFromSd() {
            Result rc = nn::fs::MountSdCardForDebug(MountName);
            if (rc != 0) {
                Logging.Log("[albw] can't mount SD card (0x%x), using built-in settings", rc);
                return -1;
            }

            nn::fs::FileHandle file;
            rc = nn::fs::OpenFile(&file, ConfigPath, nn::fs::OpenMode_Read);
            if (rc != 0) {
                Logging.Log("[albw] no %s (0x%x), using built-in settings", ConfigPath, rc);
                return -1;
            }

            long size = 0;
            long result = -1;
            rc = nn::fs::GetFileSize(&size, file);
            if (rc != 0) {
                Logging.Log("[albw] can't get size of %s (0x%x), using built-in settings", ConfigPath, rc);
            } else if (size > MaxFileSize) {
                Logging.Log("[albw] %s is %ld bytes, over the %ld-byte limit; using built-in settings", ConfigPath, size, MaxFileSize);
            } else if ((rc = nn::fs::ReadFile(file, 0, s_file_buf, static_cast<ulong>(size))) != 0) {
                Logging.Log("[albw] can't read %s (0x%x), using built-in settings", ConfigPath, rc);
            } else {
                result = size;
            }
            nn::fs::CloseFile(file);
            /* The mount is left in place; unmounting isn't declared in our nn::fs headers
               and a mounted "sd" costs nothing. */
            return result;
        }
    }

    void Load() {
        SetDefaults();

        if constexpr (UseConfigFile) {
            long size = ReadFromSd();
            if (size >= 0) {
                ParseStats stats = Parse(s_file_buf, static_cast<size_t>(size), s_settings, OnWarning);
                Logging.Log("[albw] loaded %s: %d setting(s) applied, %d line(s) ignored", ConfigPath, stats.applied, stats.warnings);
            }
        } else {
            Logging.Log("[albw] config file disabled at build time, using built-in settings");
        }

        Logging.Log("[albw] settings: server %s:%d, name '%s'",
                    s_settings.server_ip, s_settings.server_port, s_settings.player_name);
    }

    const Settings& Get() { return s_settings; }
}
