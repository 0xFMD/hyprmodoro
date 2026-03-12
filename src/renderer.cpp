#include <hyprland/src/desktop/view/Window.hpp>

#include <cairo/cairo.h>
#include <pango/pangocairo.h>

#include "hyprmodoroDecoration.hpp"
#include "pomodoro.hpp"

void HyprmodoroDecoration::renderTitleBar(CBox windowBox, PHLMONITOR pMonitor, float alpha) {
    static auto* const PTITLEFLOATINGWINDOW = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:title:floating_window")->getDataStaticPtr();
    static auto* const PMINWINDOWWIDTH      = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:window:min_width")->getDataStaticPtr();

    if (!**PTITLEFLOATINGWINDOW && m_pWindow.lock()->m_isFloating)
        return;

    if (windowBox.width <= **PMINWINDOWWIDTH) // don't render for small windows
        return;

    updateHoverOffset();
    const auto yOffset = m_hoverOffset->value();

    m_layout.container = CBox(windowBox.x + (windowBox.width - windowBox.width / 2) / 2, (windowBox.y - yOffset), windowBox.width / 2, yOffset);

    m_layout.container.translate(-pMonitor->m_position).scale(pMonitor->m_scale).round();

    if (m_pTitleTex->m_texID == 0)
        m_pTitleTex->allocate();

    const auto CAIROSURFACE = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, m_layout.container.width, m_layout.container.height);

    const auto CAIRO = cairo_create(CAIROSURFACE);

    // Clear surface
    cairo_save(CAIRO);
    cairo_set_operator(CAIRO, CAIRO_OPERATOR_CLEAR);
    cairo_paint(CAIRO);
    cairo_restore(CAIRO);

    renderTimer(CAIRO, Vector2D(m_layout.container.width, m_layout.container.height), pMonitor->m_scale);
    renderButtons(CAIRO, Vector2D(m_layout.container.width, m_layout.container.height), pMonitor->m_scale);

    cairo_surface_flush(CAIROSURFACE);
    const auto DATA = cairo_image_surface_get_data(CAIROSURFACE);

    glBindTexture(GL_TEXTURE_2D, m_pTitleTex->m_texID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

#ifndef GLES2
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
#endif

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_layout.container.width, m_layout.container.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, DATA);
    g_pHyprOpenGL->renderTexture(m_pTitleTex, m_layout.container, {.a = alpha});

    cairo_destroy(CAIRO);
    cairo_surface_destroy(CAIROSURFACE);
}

void HyprmodoroDecoration::renderTimer(cairo_t* cairo, const Vector2D& buffer, const float& scale) {
    static auto* const PTEXTCOLOR  = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:text:color")->getDataStaticPtr();
    static auto* const PFONT       = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:text:font")->getDataStaticPtr();
    static auto* const PTEXTSIZE   = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:text:size")->getDataStaticPtr();
    static auto* const PTEXTHOVER  = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:hover:text")->getDataStaticPtr();
    static auto* const PRESTPREFIX = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:text:rest_prefix")->getDataStaticPtr();
    static auto* const PWORKPREFIX = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:text:work_prefix")->getDataStaticPtr();

    const auto         textSize      = **PTEXTSIZE * scale;
    const auto         pomodoroState = g_pGlobalState->pomodoroSession->getState();
    const auto         textHover     = **PTEXTHOVER;
    const auto         timeText      = g_pGlobalState->pomodoroSession->getFormattedTime();

    const std::string  displayText = pomodoroState == State::RESTING ? std::string(*PRESTPREFIX) + " " + timeText : std::string(*PWORKPREFIX) + " " + timeText;
    CHyprColor         textColor   = CHyprColor((uint64_t)**PTEXTCOLOR);

    if (pomodoroState == State::STOPPED)
        *m_textOpacity = 0.0f;
    else if (textHover)
        *m_textOpacity = m_isNearContainer ? 1.0f : 0.0f;
    else
        *m_textOpacity = 1.0f;

    cairo_push_group(cairo);
    cairo_set_source_rgba(cairo, textColor.r, textColor.g, textColor.b, textColor.a);

    PangoLayout* layout = pango_cairo_create_layout(cairo);
    pango_layout_set_text(layout, displayText.c_str(), -1);

    PangoFontDescription* fontDesc = pango_font_description_from_string(*PFONT);
    pango_font_description_set_size(fontDesc, textSize * PANGO_SCALE);
    pango_layout_set_font_description(layout, fontDesc);
    int layoutWidth, layoutHeight;
    pango_layout_get_size(layout, &layoutWidth, &layoutHeight);

    const Vector2D textCairo = Vector2D((double)(layoutWidth / PANGO_SCALE), (double)(layoutHeight / PANGO_SCALE));

    m_layout.title = CBox((buffer.x - textCairo.x) / 2, 0, textCairo.x, textCairo.y);

    cairo_move_to(cairo, m_layout.title.x, 0);
    pango_cairo_show_layout(cairo, layout);
    g_object_unref(layout);
    pango_font_description_free(fontDesc);

    cairo_pattern_t* pattern = cairo_pop_group(cairo);
    cairo_set_source(cairo, pattern);
    cairo_paint_with_alpha(cairo, m_textOpacity->value());
    cairo_pattern_destroy(pattern);
}

void HyprmodoroDecoration::renderButtons(cairo_t* cairo, const Vector2D& buffer, const float& scale) {
    static auto* const PFONT              = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:text:font")->getDataStaticPtr();
    static auto* const PBUTTONSIZE        = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:buttons:size")->getDataStaticPtr();
    static auto* const PTEXTSIZE          = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:text:size")->getDataStaticPtr();
    static auto* const PSPACING           = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:title:spacing")->getDataStaticPtr();
    static auto* const PBUTTONSFOREGROUND = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:buttons:color:foreground")->getDataStaticPtr();
    static auto* const PBUTTONSBACKGROUND = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:buttons:color:background")->getDataStaticPtr();
    static auto* const PBUTTONHOVER       = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:hover:buttons")->getDataStaticPtr();

    const auto         currentState      = g_pGlobalState->pomodoroSession->getState();
    const std::string  PLAYICON          = g_pGlobalState->pomodoroSession->isPaused() || currentState == State::STOPPED ? "⏵" : "⏸";
    m_vButtons[ButtonAction::START].icon = PLAYICON;

    const auto     buttonSize   = **PBUTTONSIZE * scale;
    const auto     buttonHover  = **PBUTTONHOVER;
    const auto     offset       = (**PSPACING + **PTEXTSIZE) * scale;
    const auto     spacing      = buttonSize * 1.5f;
    const Vector2D centerCoords = Vector2D(buffer.x / 2, offset + (buttonSize / 2.0f));

    const bool     isFirstRender = (m_vButtons[ButtonAction::START].position->value() == Vector2D(0, 0));
    auto           setPosition   = [&isFirstRender](PHLANIMVAR<Vector2D>& pos, const Vector2D& target) {
        if (isFirstRender) {
            pos->setValueAndWarp(target); // don't animate position
        } else {
            *pos = target;
        }
    };

    setPosition(m_vButtons[ButtonAction::START].position, Vector2D(centerCoords.x - spacing - buttonSize / 2, centerCoords.y));
    setPosition(m_vButtons[ButtonAction::STOP].position, Vector2D(centerCoords.x - buttonSize / 2, centerCoords.y));
    setPosition(m_vButtons[ButtonAction::RESTART].position, Vector2D(centerCoords.x + spacing - buttonSize / 2, centerCoords.y));

    m_vButtons[ButtonAction::STOP].visible    = true;
    m_vButtons[ButtonAction::RESTART].visible = true;

    if (currentState == State::STOPPED) {
        // only render start button
        setPosition(m_vButtons[ButtonAction::START].position, Vector2D(centerCoords.x - buttonSize / 2, centerCoords.y));
        m_vButtons[ButtonAction::STOP].visible    = false;
        m_vButtons[ButtonAction::RESTART].visible = false;
    }

    for (auto& [_, button] : m_vButtons) {

        if (!button.visible)
            *button.opacity = 0.0f;
        else
            *button.opacity = (!buttonHover || m_isNearContainer) ? 1.0f : 0.0f;

        button.size             = buttonSize;
        button.color.foreground = CHyprColor((uint64_t)**PBUTTONSFOREGROUND);
        button.color.background = CHyprColor((uint64_t)**PBUTTONSBACKGROUND);
        button.hitbox           = CBox(button.position->value().x, button.position->value().y, buttonSize, buttonSize);

        const Vector2D centerButton = Vector2D(button.hitbox.x + button.hitbox.width / 2, button.hitbox.y + button.hitbox.height / 2);
        float          radius       = button.hitbox.width / 2;

        // Draw circle background
        cairo_set_source_rgba(cairo, button.color.background.r, button.color.background.g, button.color.background.b, button.color.background.a * button.opacity->value());
        cairo_arc(cairo, centerButton.x, centerButton.y, radius, 0, 2 * M_PI);
        cairo_fill(cairo);

        // Create icon layout
        PangoLayout* iconLayout = pango_cairo_create_layout(cairo);
        pango_layout_set_text(iconLayout, button.icon.c_str(), -1);

        PangoFontDescription* iconFont = pango_font_description_from_string(*PFONT);
        pango_font_description_set_size(iconFont, (button.size * 0.5f) * PANGO_SCALE);
        pango_layout_set_font_description(iconLayout, iconFont);

        // Get text extents for  centering
        PangoRectangle inkRect, logicalRect;
        pango_layout_get_extents(iconLayout, &inkRect, &logicalRect);

        const CBox     iconBox = CBox(logicalRect.x, logicalRect.y, logicalRect.width, logicalRect.height).scale(1.0f / PANGO_SCALE).round();

        const Vector2D iconTextPos = Vector2D(centerButton.x - (iconBox.width / 2) - iconBox.x, centerButton.y - (iconBox.height / 2) - iconBox.y);

        // Draw icon text
        cairo_set_source_rgba(cairo, button.color.foreground.r, button.color.foreground.g, button.color.foreground.b, button.color.foreground.a * button.opacity->value());
        cairo_move_to(cairo, iconTextPos.x, iconTextPos.y);
        pango_cairo_show_layout(cairo, iconLayout);

        g_object_unref(iconLayout);
        pango_font_description_free(iconFont);
    }
}

void HyprmodoroDecoration::renderProgressBorder(CBox windowBox, PHLMONITOR pMonitor, float alpha) {
    static auto* const BORDERCOLOR          = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:border:color")->getDataStaticPtr();
    static auto* const BORDERFLOATINGWINDOW = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprmodoro:border:floating_window")->getDataStaticPtr();
    if (!**BORDERFLOATINGWINDOW && m_pWindow.lock()->m_isFloating)
        return;

    const auto   SESSIONPROGRESS = g_pGlobalState->pomodoroSession->getProgress();

    const auto   PWINDOW  = m_pWindow.lock();
    const auto   BORDER   = PWINDOW->getRealBorderSize() * pMonitor->m_scale;
    const auto   ROUNDING = PWINDOW->rounding() * pMonitor->m_scale;

    const auto   corner       = ROUNDING + BORDER;
    const float  centerBorder = BORDER * 0.5f;
    const float  centerCorner = corner - centerBorder;

    const float  perimeter    = 2 * (windowBox.width + windowBox.height);
    const double targetLength = perimeter * SESSIONPROGRESS * 0.5;

    windowBox.translate(-pMonitor->m_position).scale(pMonitor->m_scale).round();

    const CHyprColor borderColor  = CHyprColor(**BORDERCOLOR);
    const auto       CAIROSURFACE = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, windowBox.width, windowBox.height);
    const auto       CAIRO        = cairo_create(CAIROSURFACE);
    const double     dashes[]     = {targetLength, perimeter};

    cairo_set_operator(CAIRO, CAIRO_OPERATOR_CLEAR);
    cairo_paint(CAIRO);
    cairo_set_operator(CAIRO, CAIRO_OPERATOR_OVER);

    cairo_set_source_rgba(CAIRO, borderColor.r, borderColor.g, borderColor.b, borderColor.a);
    cairo_set_line_width(CAIRO, BORDER);

    if (ROUNDING > 0) {
        // Right side path (top center -> right -> bottom center)
        cairo_new_path(CAIRO);
        cairo_move_to(CAIRO, windowBox.width * 0.5f, centerBorder); // Start at top center
        cairo_line_to(CAIRO, windowBox.width - centerCorner - centerBorder, centerBorder);
        cairo_arc(CAIRO, windowBox.width - centerCorner - centerBorder, centerCorner + centerBorder, centerCorner, -M_PI_2, 0);
        cairo_line_to(CAIRO, windowBox.width - centerBorder, windowBox.height - centerCorner - centerBorder);
        cairo_arc(CAIRO, windowBox.width - centerCorner - centerBorder, windowBox.height - centerCorner - centerBorder, centerCorner, 0, M_PI_2);
        cairo_line_to(CAIRO, windowBox.width * 0.5f, windowBox.height - centerBorder);

        cairo_set_dash(CAIRO, dashes, 2, 0);
        cairo_stroke(CAIRO);

        // Left side path (top center -> left -> bottom center)
        cairo_new_path(CAIRO);
        cairo_move_to(CAIRO, windowBox.width * 0.5f, centerBorder);
        cairo_line_to(CAIRO, centerCorner + centerBorder, centerBorder);
        cairo_arc_negative(CAIRO, centerCorner + centerBorder, centerCorner + centerBorder, centerCorner, -M_PI_2, -M_PI);
        cairo_line_to(CAIRO, centerBorder, windowBox.height - centerCorner - centerBorder);
        cairo_arc_negative(CAIRO, centerCorner + centerBorder, windowBox.height - centerCorner - centerBorder, centerCorner, -M_PI, -3 * M_PI_2);
        cairo_line_to(CAIRO, windowBox.width * 0.5f, windowBox.height - centerBorder);

        cairo_set_dash(CAIRO, dashes, 2, 0);
        cairo_stroke(CAIRO);
    } else {
        cairo_new_path(CAIRO);
        cairo_move_to(CAIRO, windowBox.width * 0.5f, centerBorder);
        cairo_line_to(CAIRO, windowBox.width - centerBorder, centerBorder);
        cairo_line_to(CAIRO, windowBox.width - centerBorder, windowBox.height - centerBorder);
        cairo_line_to(CAIRO, windowBox.width * 0.5f, windowBox.height - centerBorder);

        cairo_set_dash(CAIRO, dashes, 2, 0);
        cairo_stroke(CAIRO);

        cairo_new_path(CAIRO);
        cairo_move_to(CAIRO, windowBox.width * 0.5f, centerBorder);
        cairo_line_to(CAIRO, centerBorder, centerBorder);
        cairo_line_to(CAIRO, centerBorder, windowBox.height - centerBorder);
        cairo_line_to(CAIRO, windowBox.width * 0.5f, windowBox.height - centerBorder);

        cairo_set_dash(CAIRO, dashes, 2, 0);
        cairo_stroke(CAIRO);
    }

    if (m_pProgressTex->m_texID == 0) {
        m_pProgressTex->allocate();
    }

    cairo_set_dash(CAIRO, NULL, 0, 0);

    cairo_surface_flush(CAIROSURFACE);
    const auto DATA = cairo_image_surface_get_data(CAIROSURFACE);

    glBindTexture(GL_TEXTURE_2D, m_pProgressTex->m_texID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

#ifndef GLES2
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
#endif

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, windowBox.width, windowBox.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, DATA);
    g_pHyprOpenGL->renderTexture(m_pProgressTex, windowBox, {.a = alpha});

    cairo_destroy(CAIRO);
    cairo_surface_destroy(CAIROSURFACE);
}