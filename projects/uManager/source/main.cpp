#include <ul/man/ui/ui_MainApplication.hpp>
#include <ul/man/man_Manager.hpp>
#include <ul/man/man_Install.hpp>
#include <ul/fs/fs_Stdio.hpp>
#include <ul/sf/sf_PublicService.hpp>

ul::man::ui::MainApplication::Ref g_MainApplication;

namespace {

    bool g_IsAvailable;
    ul::Version g_GotVersion;

    void RebootSystem() {
        UL_RC_ASSERT(spsmInitialize());
        spsmShutdown(true);
    }

    bool TryAutoInstall() {
        if(ul::man::IsBasePresent() && ul::man::IsSystemActive()) {
            return false;
        }

        if(!ul::man::HasEmbeddedPayload()) {
            return false;
        }

        if(!ul::man::InstallAndActivate()) {
            return false;
        }

        RebootSystem();
        return true;
    }

    void Initialize() {
        UL_RC_ASSERT(nsInitialize());
        
        g_IsAvailable = ul::sf::IsAvailable();
        if(g_IsAvailable) {
            UL_RC_ASSERT(ul::sf::Initialize());
            UL_RC_ASSERT(ul::sf::GetVersion(&g_GotVersion));
        }
    }

    void Finalize() {
        ul::sf::Finalize();
        nsExit();
    }

}

int main() {
    ul::InitializeLogging("uDeckLaunch");
    Initialize();

    if(TryAutoInstall()) {
        Finalize();
        return 0;
    }

    auto renderer_opts = pu::ui::render::RendererInitOptions(SDL_INIT_EVERYTHING, pu::ui::render::RendererHardwareFlags);

    renderer_opts.UseImage(pu::ui::render::ImgAllFlags);

    renderer_opts.SetPlServiceType(PlServiceType_User);
    renderer_opts.AddDefaultAllSharedFonts();
    renderer_opts.AddExtraDefaultFontSize(35);

    renderer_opts.UseRomfs();

    renderer_opts.SetInputPlayerCount(1);
    renderer_opts.AddInputNpadStyleTag(HidNpadStyleSet_NpadStandard);
    renderer_opts.AddInputNpadIdType(HidNpadIdType_Handheld);
    renderer_opts.AddInputNpadIdType(HidNpadIdType_No1);

    auto renderer = pu::ui::render::Renderer::New(renderer_opts);

    g_MainApplication = ul::man::ui::MainApplication::New(renderer);
    g_MainApplication->Set(g_IsAvailable, g_GotVersion.Equals(ul::CurrentVersion), g_GotVersion);
    g_MainApplication->SetFadeBackgroundColor({ 0x0E, 0x14, 0x1B, 0xFF });
    UL_RC_ASSERT(g_MainApplication->Load());
    g_MainApplication->ShowWithFadeIn();

    Finalize();
    return 0;
}
