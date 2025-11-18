#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/managers/animation/AnimationManager.hpp>
#include <hyprland/src/config/ConfigManager.hpp>

#include "hyprmodoroDecoration.hpp"
#include "HyprmodoroPassElement.hpp"
#include "pomodoro.hpp"

HyprmodoroDecoration::HyprmodoroDecoration(PHLWINDOW pWindow) : IHyprWindowDecoration(pWindow), m_pWindow(pWindow) {
    m_pTitleTex    = makeShared<CTexture>();
    m_pProgressTex = makeShared<CTexture>();
    setupButtons();

    m_pMouseButtonCallback = HyprlandAPI::registerCallbackDynamic(
        PHANDLE, "mouseButton", [&](void* self, SCallbackInfo& info, std::any param) { onMouseDown(info, std::any_cast<IPointer::SButtonEvent>(param)); });

    g_pAnimationManager->createAnimation(25.0f, m_hoverOffset, g_pConfigManager->getAnimationPropertyConfig("windowsMove"), AVARDAMAGE_ENTIRE);
    g_pAnimationManager->createAnimation(0.0f, m_textOpacity, g_pConfigManager->getAnimationPropertyConfig("fadeOut"), AVARDAMAGE_ENTIRE);
}

HyprmodoroDecoration::~HyprmodoroDecoration() {
    m_pMouseButtonCallback = nullptr;
    m_vButtons.clear();
    std::erase(g_pGlobalState->decorations, m_self);
}

SDecorationPositioningInfo HyprmodoroDecoration::getPositioningInfo() {
    static auto* const         PISENABLED      = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:enabled")->getDataStaticPtr();
    static auto* const         PTITLEISENABLED = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:title:enabled")->getDataStaticPtr();
    static auto* const         PMINWINDOWWIDTH = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:window:min_width")->getDataStaticPtr();

    static auto* const         PRESERVESPACEALL = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:title:reserve_space_all")->getDataStaticPtr();
    static auto* const         PTITLEALLWINDOWS = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:title:all_windows")->getDataStaticPtr();
    static auto* const         PTEXTHOVER       = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:hover:text")->getDataStaticPtr();
    static auto* const         PBUTTONHOVER     = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:hover:buttons")->getDataStaticPtr();
    static auto* const         PTITLEMARGIN     = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:title:margin")->getDataStaticPtr();
    static auto* const         PBUTTONSIZE      = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:buttons:size")->getDataStaticPtr();
    static auto* const         PSPACING         = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:title:spacing")->getDataStaticPtr();
    static auto* const         PTEXTSIZE        = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:text:size")->getDataStaticPtr();
    static auto* const         PWINDOWPADDING   = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:window:padding")->getDataStaticPtr();

    SDecorationPositioningInfo info;
    info.edges    = DECORATION_EDGE_BOTTOM | DECORATION_EDGE_LEFT | DECORATION_EDGE_RIGHT | DECORATION_EDGE_TOP;
    info.policy   = DECORATION_POSITION_STICKY;
    info.reserved = true;
    info.priority = 500;

    if (!**PISENABLED || !**PTITLEISENABLED)
        return info;

    const auto PWINDOW     = m_pWindow.lock();
    const auto PLASTWINDOW = g_pCompositor->m_lastWindow.lock();

    if (!**PRESERVESPACEALL && !**PTITLEALLWINDOWS && PWINDOW != PLASTWINDOW) // only focused windows
        return info;

    if (PWINDOW->m_size.x <= **PMINWINDOWWIDTH)
        return info;

    const auto textSize      = **PTEXTSIZE;
    const auto buttonSize    = **PBUTTONSIZE;
    const auto spacing       = **PSPACING;
    const auto buttonsSpace  = spacing + buttonSize;
    const auto textHover     = **PTEXTHOVER;
    const auto buttonHover   = **PBUTTONHOVER;
    const auto windowPadding = **PWINDOWPADDING;
    const auto POMDOOROSTATE = g_pGlobalState->pomodoroSession->getState();
    float      yOffset       = 0.0f;

    if (textHover && buttonHover && !m_isNearContainer) {
        yOffset = windowPadding;
    } else if ((buttonHover && m_isNearContainer) || !buttonHover) {
        yOffset = POMDOOROSTATE == State::STOPPED ? (windowPadding + buttonSize + spacing) : (windowPadding + buttonSize + spacing + textSize);
    } else if (POMDOOROSTATE != State::STOPPED) {
        yOffset = windowPadding + textSize + spacing;
    } else {
        yOffset = windowPadding;
    }

    info.desiredExtents = {{0.0, yOffset}, {0, 0}};
    return info;
}

void HyprmodoroDecoration::onPositioningReply(const SDecorationPositioningReply& reply) {
    m_bAssignedGeometry = reply.assignedGeometry;
}

uint64_t HyprmodoroDecoration::getDecorationFlags() {
    return DECORATION_PART_OF_MAIN_WINDOW | DECORATION_ALLOWS_MOUSE_INPUT;
}

eDecorationLayer HyprmodoroDecoration::getDecorationLayer() {
    return DECORATION_LAYER_OVER;
}

eDecorationType HyprmodoroDecoration::getDecorationType() {
    return DECORATION_CUSTOM;
}

std::string HyprmodoroDecoration::getDisplayName() {
    return "hyprmodoro";
}

CBox HyprmodoroDecoration::assignedBoxGlobal() {

    CBox       box     = m_bAssignedGeometry;
    const auto PWINDOW = m_pWindow.lock();

    box.translate(g_pDecorationPositioner->getEdgeDefinedPoint(DECORATION_EDGE_BOTTOM | DECORATION_EDGE_LEFT | DECORATION_EDGE_RIGHT | DECORATION_EDGE_TOP, PWINDOW));

    const auto PWORKSPACE      = PWINDOW->m_workspace;
    const auto WORKSPACEOFFSET = PWORKSPACE && !PWINDOW->m_pinned ? PWORKSPACE->m_renderOffset->value() : Vector2D();

    return box.translate(WORKSPACEOFFSET);
}

void HyprmodoroDecoration::onMouseDown(SCallbackInfo& info, IPointer::SButtonEvent e) {
    if (!isValidInput())
        return;

    if (e.state != WL_POINTER_BUTTON_STATE_PRESSED || e.button != BTN_LEFT)
        return;

    const auto         cursorPos = cursorRelativeToContainer();

    static const auto* PSKIPONCLICK = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:text:skip_on_click")->getDataStaticPtr();

    const auto         isRunning = g_pGlobalState->pomodoroSession->getState() == State::WORKING || g_pGlobalState->pomodoroSession->getState() == State::RESTING;

    if (m_layout.title.containsPoint(cursorPos) && **PSKIPONCLICK && isRunning) {
        HyprlandAPI::addNotification(PHANDLE, std::format("[hyprmodoro] Skipped {}", isRunning ? "work" : "rest"), CHyprColor{0.0, 1.0, 0.0, 1.0}, 3000);
        g_pGlobalState->pomodoroSession->skip();
        return;
    }

    for (const auto& [buttonAction, button] : m_vButtons) {
        if (button.visible && button.hitbox.containsPoint(cursorPos)) {
            handleButtonClick(buttonAction);
            info.cancelled = true;
            return;
        }
    }
}

void HyprmodoroDecoration::handleButtonClick(ButtonAction buttonAction) {
    const auto currentState = g_pGlobalState->pomodoroSession->getState();
    if (buttonAction == ButtonAction::START) {
        if ((currentState == State::WORKING || currentState == State::RESTING) && !g_pGlobalState->pomodoroSession->isPaused())
            g_pGlobalState->pomodoroSession->pause();
        else if (g_pGlobalState->pomodoroSession->isPaused())
            g_pGlobalState->pomodoroSession->resume();
        else
            g_pGlobalState->pomodoroSession->start();

    } else if (buttonAction == ButtonAction::STOP)
        g_pGlobalState->pomodoroSession->stop();

    else if (buttonAction == ButtonAction::RESTART)
        g_pGlobalState->pomodoroSession->reset();
}

void HyprmodoroDecoration::draw(PHLMONITOR pMonitor, const float& a) {
    if (!validMapped(m_pWindow))
        return;

    const auto PWINDOW = m_pWindow.lock();

	if (!PWINDOW->m_ruleApplicator->decorate().valueOrDefault())
        return;

    if (!PWINDOW->m_workspace->isVisible() || PWINDOW->isHidden())
        return;

    HyprmodoroPassElement::SBorderPPData data;
    data.deco = this;

    g_pHyprRenderer->m_renderPass.add(makeUnique<HyprmodoroPassElement>(data));
}

void HyprmodoroDecoration::drawPass(PHLMONITOR pMonitor, const float& a) {
    static auto* const PENABLED      = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:enabled")->getDataStaticPtr();
    static auto* const PHOVERTITLE   = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:hover:text")->getDataStaticPtr();
    static auto* const PHOVERBUTTONS = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:hover:buttons")->getDataStaticPtr();

    static auto* const PBORDERENABLED    = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:border:enabled")->getDataStaticPtr();
    static auto* const PTITLEENABLED     = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:title:enabled")->getDataStaticPtr();
    static auto* const PBORDERALLWINDOWS = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:border:all_windows")->getDataStaticPtr();
    static auto* const PTITLEALLWINDOWS  = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:title:all_windows")->getDataStaticPtr();

    if (!**PENABLED)
        return;
    const auto windowBox = assignedBoxGlobal();
    if (windowBox.width <= 0 || windowBox.height <= 0)
        return;

    const auto SESSIONPROGRESS = g_pGlobalState->pomodoroSession->getProgress();
    const auto PWINDOW         = m_pWindow.lock();
    const auto PLASTWINDOW     = g_pCompositor->m_lastWindow.lock();
    if (**PBORDERENABLED && SESSIONPROGRESS <= 1.0f && SESSIONPROGRESS > 0.0f) {
        if (**PBORDERALLWINDOWS || PWINDOW == PLASTWINDOW) {
            renderProgressBorder(pMonitor, a);
        }
    }

    if (**PTITLEENABLED) {

        if ((**PTITLEALLWINDOWS && (!pMonitor->m_activeWorkspace->m_hasFullscreenWindow || PWINDOW->isFullscreen())) || PWINDOW == PLASTWINDOW) {
            renderTitleBar(pMonitor, a);
        }
    }
    updateWindow(m_pWindow.lock());
    damageEntire();
}

void HyprmodoroDecoration::updateWindow(PHLWINDOW pWindow) {
    static auto* const PISTITLEENABLED = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:title:enabled")->getDataStaticPtr();

    if (!**PISTITLEENABLED || !g_pGlobalState->pomodoroSession)
        return;

    static auto* const PTITLEALLWINDOWS = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:title:all_windows")->getDataStaticPtr();
    static auto* const PHOVERTITLE      = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:hover:text")->getDataStaticPtr();
    static auto* const PHOVERBUTTONS    = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:hover:buttons")->getDataStaticPtr();
    const auto         PWINDOW          = m_pWindow.lock();
    const auto         PLASTWINDOW      = g_pCompositor->m_lastWindow.lock();
    const auto         PMONITOR         = PWINDOW->m_monitor.lock();
    bool               hoverState       = isHoveringTitle(assignedBoxGlobal(), PMONITOR->m_scale);
    bool               needReposition   = false;

    if (!**PTITLEALLWINDOWS && PWINDOW != PLASTWINDOW) {
        needReposition = true;
    }

    if (g_pGlobalState->pomodoroSession->getState() != g_pGlobalState->pomodoroSession->getLastState())
        needReposition = true;

    if (hoverState != m_isNearContainer && (**PHOVERTITLE || **PHOVERBUTTONS)) {
        m_isNearContainer = hoverState;
        needReposition    = true;
    }

    if (needReposition) {
        g_pDecorationPositioner->repositionDeco(this);
    }
}

void HyprmodoroDecoration::damageEntire() {
    auto box = assignedBoxGlobal();
    box.y -= m_layout.container.height;
    box.height += m_layout.container.height;
    g_pHyprRenderer->damageBox(box);
}
