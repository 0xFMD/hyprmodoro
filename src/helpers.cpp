#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/protocols/LayerShell.hpp>
#include <hyprland/src/managers/animation/AnimationManager.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/state/FocusState.hpp>

#include "hyprmodoroDecoration.hpp"

Vector2D HyprmodoroDecoration::cursorRelativeToContainer() {
    const auto PMONITOR  = m_pWindow.lock()->m_monitor.lock();
    auto       cursorPos = g_pInputManager->getMouseCoordsInternal() - PMONITOR->m_position;
    cursorPos.x *= PMONITOR->m_scale;
    cursorPos.y *= PMONITOR->m_scale;
    return cursorPos - m_layout.container.pos();
}

bool HyprmodoroDecoration::isHoveringTitle(const CBox& windowBox, const float& scale) {
    if (!isValidInput())
        return false;

    static auto* const PHOVERTITLE   = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:hover:text")->getDataStaticPtr();
    static auto* const PHOVERBUTTONS = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:hover:buttons")->getDataStaticPtr();
    static const auto* PHOVERHEIGHT  = (Hyprlang::FLOAT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:hover:height")->getDataStaticPtr();
    static const auto* PHOVERWIDTH   = (Hyprlang::FLOAT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:hover:width")->getDataStaticPtr();

    const auto         cursorPos = cursorRelativeToContainer();

    const float        hoverWidth  = windowBox.width * scale * (**PHOVERWIDTH / 100.0f);
    const float        hoverHeight = windowBox.height * scale * (**PHOVERHEIGHT / 100.0f);

    CBox               hoverBox = CBox((m_layout.container.width - hoverWidth) * 0.5f, -hoverHeight, hoverWidth, (hoverHeight * 2.0f) + m_layout.container.height);

    return hoverBox.containsPoint(cursorPos);
}

void HyprmodoroDecoration::updateHoverOffset() {
    static auto* const PTEXTHOVER   = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:hover:text")->getDataStaticPtr();
    static auto* const PBUTTONHOVER = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:hover:buttons")->getDataStaticPtr();
    static auto* const PTITLEMARGIN = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:title:margin")->getDataStaticPtr();
    static auto* const PBUTTONSIZE  = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:buttons:size")->getDataStaticPtr();
    static auto* const PSPACING     = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:title:spacing")->getDataStaticPtr();
    static auto* const PTEXTSIZE    = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:text:size")->getDataStaticPtr();

    const auto         buttonsSpace = **PSPACING + **PBUTTONSIZE;
    const auto         textHover    = **PTEXTHOVER;
    const auto         buttonHover  = **PBUTTONHOVER;
    const auto         textSize     = **PTEXTSIZE;

    *m_hoverOffset = **PTITLEMARGIN + textSize + (((buttonHover && m_isNearContainer) || !buttonHover) ? buttonsSpace : 0.0f);
}

bool HyprmodoroDecoration::isValidInput() {
    static auto* const PENABLED = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:enabled")->getDataStaticPtr();

    if (!**PENABLED)
        return false;

    const auto PWINDOW = m_pWindow.lock();

    if (!m_pWindow->m_workspace || !validMapped(m_pWindow) || !m_pWindow->m_workspace->isVisible() || !g_pInputManager->m_exclusiveLSes.empty() ||
        (g_pSeatManager->m_seatGrab && !g_pSeatManager->m_seatGrab->accepts(m_pWindow->m_wlSurface->resource())))
        return false;

    const auto WINDOWATCURSOR = g_pCompositor->vectorToWindowUnified(g_pInputManager->getMouseCoordsInternal(), RESERVED_EXTENTS | INPUT_EXTENTS | ALLOW_FLOATING);

    if (WINDOWATCURSOR != PWINDOW && PWINDOW != Desktop::focusState()->window())
        return false;

    // Check if input is on top or overlay shell layers
    auto     PMONITOR     = Desktop::focusState()->monitor();
    PHLLS    foundSurface = nullptr;
    Vector2D surfaceCoords;

    // Check top layer
    g_pCompositor->vectorToLayerSurface(g_pInputManager->getMouseCoordsInternal(), &PMONITOR->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_TOP], &surfaceCoords, &foundSurface);

    if (foundSurface)
        return false;

    // Check overlay layer
    g_pCompositor->vectorToLayerSurface(g_pInputManager->getMouseCoordsInternal(), &PMONITOR->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY], &surfaceCoords,
                                        &foundSurface);

    if (foundSurface)
        return false;

    // if (!VECINRECT(cursorRelativeToContainer(), 0, 0, m_layout.container.width, m_layout.container.height))
    //     return false;

    return true;
}

void HyprmodoroDecoration::setupButtons() {
    static auto* const PBUTTONSIZE       = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:buttons:size")->getDataStaticPtr();
    static auto* const PBUTTONFOREGROUND = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:buttons:color:foreground")->getDataStaticPtr();
    static auto* const PBUTTONBACKGROUND = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:buttons:color:background")->getDataStaticPtr();

    m_vButtons[ButtonAction::STOP]    = {.action = ButtonAction::STOP,
                                         .color =
                                             Button::ButtonColor{.foreground = CHyprColor((uint64_t)**PBUTTONFOREGROUND), .background = CHyprColor((uint64_t)**PBUTTONBACKGROUND)},
                                         .size = (float)**PBUTTONSIZE,
                                         .icon = "⏹"};
    m_vButtons[ButtonAction::START]   = {.action = ButtonAction::START,
                                         .color =
                                             Button::ButtonColor{.foreground = CHyprColor((uint64_t)**PBUTTONFOREGROUND), .background = CHyprColor((uint64_t)**PBUTTONBACKGROUND)},
                                         .size = (float)**PBUTTONSIZE,
                                         .icon = "⏵"};
    m_vButtons[ButtonAction::RESTART] = {.action = ButtonAction::RESTART,
                                         .color =
                                             Button::ButtonColor{.foreground = CHyprColor((uint64_t)**PBUTTONFOREGROUND), .background = CHyprColor((uint64_t)**PBUTTONBACKGROUND)},
                                         .size = (float)**PBUTTONSIZE,
                                         .icon = "↻"};

    g_pAnimationManager->createAnimation(0.0f, m_vButtons[ButtonAction::STOP].opacity, g_pConfigManager->getAnimationPropertyConfig("fadeOut"), AVARDAMAGE_ENTIRE);
    g_pAnimationManager->createAnimation(0.0f, m_vButtons[ButtonAction::START].opacity, g_pConfigManager->getAnimationPropertyConfig("fadeOut"), AVARDAMAGE_ENTIRE);
    g_pAnimationManager->createAnimation(0.0f, m_vButtons[ButtonAction::RESTART].opacity, g_pConfigManager->getAnimationPropertyConfig("fadeOut"), AVARDAMAGE_ENTIRE);

    g_pAnimationManager->createAnimation(Vector2D(0, 0), m_vButtons[ButtonAction::START].position, g_pConfigManager->getAnimationPropertyConfig("windowsMove"), AVARDAMAGE_ENTIRE);
    g_pAnimationManager->createAnimation(Vector2D(0, 0), m_vButtons[ButtonAction::STOP].position, g_pConfigManager->getAnimationPropertyConfig("windowsMove"), AVARDAMAGE_ENTIRE);
    g_pAnimationManager->createAnimation(Vector2D(0, 0), m_vButtons[ButtonAction::RESTART].position, g_pConfigManager->getAnimationPropertyConfig("windowsMove"),
                                         AVARDAMAGE_ENTIRE);
}
