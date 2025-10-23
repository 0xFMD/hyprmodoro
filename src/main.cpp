#define WLR_USE_UNSTABLE

#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/Compositor.hpp>

#include <string>

#include "hyprmodoroDecoration.hpp"
#include "globals.hpp"
#include "pomodoro.hpp"

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

// Dispatchers
SDispatchResult startTimer(std::string) {
    if (g_pGlobalState->pomodoroSession && g_pGlobalState->pomodoroSession->getState() == STOPPED) {
        g_pGlobalState->pomodoroSession->start();
        return SDispatchResult{.success = true};
    }

    return SDispatchResult{.error = "hyprmodoro:start: failed to start timer"};
}

SDispatchResult stopTimer(std::string) {
    if (g_pGlobalState->pomodoroSession) {
        g_pGlobalState->pomodoroSession->stop();
        return SDispatchResult{.success = true};
    }
    return SDispatchResult{.error = "hyprmodoro:stop: failed to stop timer"};
}

SDispatchResult togglePauseTimer(std::string) {
    if (g_pGlobalState->pomodoroSession) {
        g_pGlobalState->pomodoroSession->isPaused() ? g_pGlobalState->pomodoroSession->resume() : g_pGlobalState->pomodoroSession->pause();
        return SDispatchResult{.success = true};
    }

    return SDispatchResult{.error = "hyprmodoro:pause: failed to pause timer"};
}

SDispatchResult setTimer(std::string data) {
    if (g_pGlobalState->pomodoroSession) {
        g_pGlobalState->pomodoroSession->setSessionLength(std::stoi(data));
        return SDispatchResult{.success = true};
    }
    return SDispatchResult{.error = "hyprmodoro:set: failed to set timer"};
}

SDispatchResult setRest(std::string data) {
    if (g_pGlobalState->pomodoroSession) {
        g_pGlobalState->pomodoroSession->setRestLength(std::stoi(data));
        return SDispatchResult{.success = true};
    }
    return SDispatchResult{.error = "hyprmodoro:setRest: failed to set rest duration"};
}

SDispatchResult skip(std::string) {
    if (g_pGlobalState->pomodoroSession) {
        g_pGlobalState->pomodoroSession->skip();
        return SDispatchResult{.success = true};
    }
    return SDispatchResult{.error = "hyprmodoro:skip: failed to skip timer"};
}

// hyprctl commands
std::string getTime(eHyprCtlOutputFormat, std::string) {
    if (!g_pGlobalState->pomodoroSession)
        return std::string("00:00");
    return g_pGlobalState->pomodoroSession->getFormattedTime();
}

std::string getState(eHyprCtlOutputFormat, std::string) {
    if (!g_pGlobalState->pomodoroSession)
        return std::string("STOPPED");
    const std::string states[] = {"STOPPED", "WORKING", "RESTING", "FINISHED"};
    return states[g_pGlobalState->pomodoroSession->getState()];
}

std::string getProgress(eHyprCtlOutputFormat, std::string) {
    if (!g_pGlobalState->pomodoroSession)
        return std::string("0.0");
    return std::to_string(g_pGlobalState->pomodoroSession->getProgress());
}

// handlers
void onWindowOpen(void* self, std::any data) {
    const auto         PWINDOW = std::any_cast<PHLWINDOW>(data);

    static auto* const IS_HYPRMODORO_ENABLED = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:enabled")->getDataStaticPtr();
    if (!(**IS_HYPRMODORO_ENABLED) || PWINDOW == nullptr)
        return;

    if (std::ranges::any_of(PWINDOW->m_windowDecorations, [](const auto& d) { return d->getDisplayName() == "hyprmodoro"; }))
        return;

    auto decoration = makeUnique<HyprmodoroDecoration>(PWINDOW);
    g_pGlobalState->decorations.emplace_back(decoration);
    decoration->m_self = decoration;
    HyprlandAPI::addWindowDecoration(PHANDLE, PWINDOW, std::move(decoration));
}

void onWindowClose(void* self, std::any data) {
    if (g_pGlobalState) {
        g_pGlobalState->decorations.erase(std::remove_if(g_pGlobalState->decorations.begin(), g_pGlobalState->decorations.end(), [](const auto& wp) { return wp.expired(); }),
                                          g_pGlobalState->decorations.end());
    }
}

void onConfigReload(void* self, SCallbackInfo& info, std::any data) {
    static auto* const SESSIONLENGTH = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:work_duration")->getDataStaticPtr();
    static auto* const RESTLENGTH    = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:rest_duration")->getDataStaticPtr();

    if (g_pGlobalState->pomodoroSession) {
        g_pGlobalState->pomodoroSession->setSessionLength(**SESSIONLENGTH);
        g_pGlobalState->pomodoroSession->setRestLength(**RESTLENGTH);
    }
}

// init plugin

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    const std::string COMPOSITOR_HASH = __hyprland_api_get_hash();
    const std::string CLIENT_HASH = __hyprland_api_get_client_hash();

    if (COMPOSITOR_HASH != CLIENT_HASH) {
        HyprlandAPI::addNotification(PHANDLE, "[hyprmodoro] Failure in initialization: Version mismatch (headers ver is not equal to running hyprland ver)",
                                     CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error("[hyprmodoro] Version mismatch");
    }

    g_pGlobalState = makeUnique<SGlobalState>();

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprmodoro:enabled", Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprmodoro:work_duration", Hyprlang::INT{25});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprmodoro:rest_duration", Hyprlang::INT{5});

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprmodoro:border:enabled", Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprmodoro:border:floating_window", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprmodoro:border:all_windows", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprmodoro:border:color", Hyprlang::INT{*configStringToInt("rgba(33333388)")});

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprmodoro:text:color", Hyprlang::INT{*configStringToInt("rgba(ffffffff)")});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprmodoro:text:font", Hyprlang::STRING{"Sans"});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprmodoro:text:size", Hyprlang::INT{17});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprmodoro:text:work_prefix", Hyprlang::STRING{"🍅"});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprmodoro:text:rest_prefix", Hyprlang::STRING{"☕"});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprmodoro:text:skip_on_click", Hyprlang::INT{1});

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprmodoro:buttons:size", Hyprlang::INT{17});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprmodoro:buttons:color:foreground", Hyprlang::INT{*configStringToInt("rgba(ffffffff)")});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprmodoro:buttons:color:background", Hyprlang::INT{*configStringToInt("rgba(ffffff44)")});

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprmodoro:title:enabled", Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprmodoro:title:reserve_space_all", Hyprlang::INT{0});

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprmodoro:title:floating_window", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprmodoro:title:all_windows", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprmodoro:title:margin", Hyprlang::INT{15});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprmodoro:title:spacing", Hyprlang::INT{8});

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprmodoro:hover:text", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprmodoro:hover:buttons", Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprmodoro:hover:height", Hyprlang::FLOAT{10});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprmodoro:hover:width", Hyprlang::FLOAT{20});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprmodoro:window:min_width", Hyprlang::INT{300});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprmodoro:window:padding", Hyprlang::INT{0});

    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprmodoro:start", startTimer);
    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprmodoro:stop", stopTimer);
    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprmodoro:pause", togglePauseTimer);
    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprmodoro:set", setTimer);
    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprmodoro:setRest", setRest);
    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprmodoro:skip", skip);

    HyprlandAPI::registerHyprCtlCommand(PHANDLE, SHyprCtlCommand{"hyprmodoro:getProgress", true, getProgress});
    HyprlandAPI::registerHyprCtlCommand(PHANDLE, SHyprCtlCommand{"hyprmodoro:getState", true, getState});
    HyprlandAPI::registerHyprCtlCommand(PHANDLE, SHyprCtlCommand{"hyprmodoro:getTime", true, getTime});

    static auto* const SESSIONLENGTH = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:work_duration")->getDataStaticPtr();
    static auto* const RESTLENGTH    = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:rest_duration")->getDataStaticPtr();

    g_pGlobalState->pomodoroSession = makeUnique<Pomodoro>(**SESSIONLENGTH, **RESTLENGTH);

    static auto configReloadCallback = HyprlandAPI::registerCallbackDynamic(PHANDLE, "configReloaded", onConfigReload);

    static auto closeWindowCallback =
        HyprlandAPI::registerCallbackDynamic(PHANDLE, "closeWindow", [&](void* self, SCallbackInfo& info, std::any data) { onWindowClose(self, data); });

    static auto openWindowCallback = HyprlandAPI::registerCallbackDynamic(PHANDLE, "openWindow", [&](void* self, SCallbackInfo& info, std::any data) { onWindowOpen(self, data); });

    for (auto& window : g_pCompositor->m_windows) {
        if (window->isHidden() || !window->m_isMapped)
            continue;
        onWindowOpen(nullptr, window);
    }

    return {"hyprmodoro", "A Pomodoro timer plugin for Hyprland", "0xFMD", "0.1"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    for (auto& m : g_pCompositor->m_monitors)
        m->m_scheduledRecalc = true;

    g_pHyprRenderer->m_renderPass.removeAllOfType("HyprmodoroPassElement");
}
