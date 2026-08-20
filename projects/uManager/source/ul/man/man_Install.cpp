#include <ul/man/man_Install.hpp>
#include <ul/man/man_Manager.hpp>
#include <ul/fs/fs_Stdio.hpp>
#include <zip.h>

namespace ul::man {

    namespace {

        constexpr const char ZipPrefix[] = "SdOut/";

        bool ExtractZipEntry(zip_t *zip_file, u32 index, InstallProgressCallback on_progress, u32 total) {
            if(zip_entry_openbyindex(zip_file, index) != 0) {
                return false;
            }

            std::string entry_name = zip_entry_name(zip_file);
            if(entry_name.rfind(ZipPrefix, 0) == 0) {
                entry_name = entry_name.substr(sizeof(ZipPrefix) - 1);
            }
            else if(entry_name.rfind("SdOut\\", 0) == 0) {
                entry_name = entry_name.substr(6);
            }
            for(auto &ch : entry_name) {
                if(ch == '\\') {
                    ch = '/';
                }
            }

            while(!entry_name.empty() && entry_name.back() == '/') {
                entry_name.pop_back();
            }

            const auto is_dir = zip_entry_isdir(zip_file);
            if(!entry_name.empty()) {
                const auto sd_path = std::string("sdmc:/") + entry_name;
                if(is_dir) {
                    fs::EnsureCreateDirectory(sd_path);
                }
                else {
                    fs::EnsureCreateDirectory(fs::GetBaseDirectory(sd_path));
                    auto f = fopen(sd_path.c_str(), "wb");
                    if(!f) {
                        zip_entry_close(zip_file);
                        return false;
                    }
                    void *read_buf = nullptr;
                    size_t read_buf_size = 0;
                    const auto read_size = zip_entry_read(zip_file, &read_buf, &read_buf_size);
                    if(read_size > 0 && read_buf) {
                        fwrite(read_buf, read_size, 1, f);
                    }
                    if(read_buf) {
                        free(read_buf);
                    }
                    fclose(f);
                }
            }

            zip_entry_close(zip_file);
            if(on_progress) {
                on_progress(static_cast<double>(index + 1), static_cast<double>(total));
            }
            return true;
        }

    }

    bool HasEmbeddedPayload() {
        return fs::ExistsFile(EmbeddedPayloadPath);
    }

    bool InstallFromZip(const char *zip_path, InstallProgressCallback on_progress) {
        auto zip_file = zip_open(zip_path, 0, 'r');
        if(!zip_file) {
            return false;
        }

        const auto file_count = zip_entries_total(zip_file);
        if(file_count <= 0) {
            zip_close(zip_file);
            return false;
        }

        if(on_progress) {
            on_progress(0, static_cast<double>(file_count));
        }

        auto ok = true;
        for(u32 i = 0; i < file_count && ok; i++) {
            ok = ExtractZipEntry(zip_file, i, on_progress, file_count);
        }

        zip_close(zip_file);
        return ok;
    }

    bool InstallEmbeddedPayload(InstallProgressCallback on_progress) {
        if(!HasEmbeddedPayload()) {
            return false;
        }

        fs::CopyFile(EmbeddedPayloadPath, TemporaryReleaseZipPath);
        if(!fs::ExistsFile(TemporaryReleaseZipPath)) {
            return false;
        }

        const auto ok = InstallFromZip(TemporaryReleaseZipPath, on_progress);
        remove(TemporaryReleaseZipPath);
        return ok && IsBasePresent();
    }

    bool InstallAndActivate(InstallProgressCallback on_progress) {
        if(!IsBasePresent()) {
            if(!InstallEmbeddedPayload(on_progress)) {
                return false;
            }
        }

        EnsureRomfsSlot();
        RemoveAlbumOverlay();

        if(!IsSystemActive()) {
            ActivateSystem();
        }

        return IsBasePresent() && IsSystemActive();
    }

    void EnsureRomfsSlot() {
        fs::EnsureCreateDirectory("sdmc:/atmosphere/contents/0100000000001000/romfs");
    }

    void RemoveAlbumOverlay() {
        const char *path = "sdmc:/atmosphere/contents/010000000000100D";
        if(fs::ExistsDirectory(path)) {
            fs::DeleteDirectory(path);
        }
    }

}
