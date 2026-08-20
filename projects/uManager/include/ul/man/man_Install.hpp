
#pragma once
#include <ul/ul_Include.hpp>
#include <functional>

namespace ul::man {

    using InstallProgressCallback = std::function<void(double done, double total)>;

    constexpr const char EmbeddedPayloadPath[] = "romfs:/payload.zip";
    constexpr const char TemporaryReleaseZipPath[] = "sdmc:/ulaunch_tmp.zip";

    bool InstallFromZip(const char *zip_path, InstallProgressCallback on_progress);
    bool InstallEmbeddedPayload(InstallProgressCallback on_progress = nullptr);
    bool HasEmbeddedPayload();
    bool InstallAndActivate(InstallProgressCallback on_progress = nullptr);
    void EnsureRomfsSlot();
    void RemoveAlbumOverlay();

}
