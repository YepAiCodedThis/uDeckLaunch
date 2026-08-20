#include "deck_lang.h"

#include <ul/menu/ui/ui_Common.hpp>
#include <ul/cfg/cfg_Config.hpp>
#include <ul/fs/fs_Stdio.hpp>
#include <ul/util/util_Json.hpp>
#include <switch.h>
#include <cstring>
#include <cstdio>

extern ul::menu::ui::GlobalSettings g_GlobalSettings;

namespace {

    constexpr const char *kCodes[] = {
        "system", "en", "de", "es", "fr", "it", "pt-BR", "ko"
    };
    constexpr const char *kNames[] = {
        "lang_system", "lang_en", "lang_de", "lang_es", "lang_fr", "lang_it", "lang_ptbr", "lang_ko"
    };
    constexpr int kCount = 8;

    char g_ring[16][192];
    int g_ri;

    const char *Store(const std::string &s) {
        char *b = g_ring[g_ri++ & 15];
        std::snprintf(b, 192, "%s", s.c_str());
        return b;
    }

    std::string DetectSystemCode() {
        u64 lang_code = 0;
        if(R_FAILED(setGetLanguageCode(&lang_code))) {
            return "en";
        }
        const char *sys = reinterpret_cast<char *>(&lang_code);
        if(std::strcmp(sys, "en-US") == 0 || std::strcmp(sys, "en-GB") == 0) {
            return "en";
        }
        if(std::strcmp(sys, "fr-CA") == 0) {
            return "fr";
        }
        if(std::strcmp(sys, "es-419") == 0) {
            return "es";
        }
        if(std::strcmp(sys, "pt") == 0) {
            return "pt-BR";
        }
        for(int i = 1; i < kCount; i++) {
            if(std::strcmp(sys, kCodes[i]) == 0) {
                return kCodes[i];
            }
        }
        if(std::strncmp(sys, "pt", 2) == 0) {
            return "pt-BR";
        }
        return "en";
    }

    void LoadPair(const std::string &code) {
        const auto en_path = std::string(ul::BuiltinMenuLanguagesPath) + "/en.json";
        ul::util::LoadJSONFromFile(g_GlobalSettings.default_lang, en_path);
        auto path = std::string(ul::BuiltinMenuLanguagesPath) + "/" + code + ".json";
        if(code == "en" || R_FAILED(ul::util::LoadJSONFromFile(g_GlobalSettings.main_lang, path))) {
            g_GlobalSettings.main_lang = g_GlobalSettings.default_lang;
        }
    }

}

extern "C" {

    void dh_lang_apply(const char *pref) {
        std::string code = "en";
        if(!pref || !pref[0] || std::strcmp(pref, "system") == 0) {
            code = DetectSystemCode();
        }
        else {
            code = pref;
        }
        LoadPair(code);
    }

    int dh_lang_count(void) {
        return kCount;
    }

    int dh_lang_index(const char *pref) {
        if(!pref || !pref[0] || std::strcmp(pref, "system") == 0) {
            return 0;
        }
        for(int i = 1; i < kCount; i++) {
            if(std::strcmp(pref, kCodes[i]) == 0) {
                return i;
            }
        }
        return 0;
    }

    void dh_lang_code(int index, char *out, size_t n) {
        if(index < 0 || index >= kCount) {
            index = 0;
        }
        std::snprintf(out, n, "%s", kCodes[index]);
    }

    const char *dh_lang_name(int index) {
        if(index < 0 || index >= kCount) {
            index = 0;
        }
        return dh_lang(kNames[index]);
    }

    const char *dh_lang(const char *key) {
        if(!key) {
            return "";
        }
        return Store(ul::cfg::GetLanguageString(g_GlobalSettings.main_lang, g_GlobalSettings.default_lang, key));
    }

}
