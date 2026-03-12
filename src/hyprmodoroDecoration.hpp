#pragma once

#define WLR_USE_UNSTABLE

#include <hyprland/src/render/decorations/IHyprWindowDecoration.hpp>
#include <hyprland/src/devices/IPointer.hpp>
#include <hyprland/src/event/EventBus.hpp>

#include "HyprmodoroPassElement.hpp"
#include "globals.hpp"

enum ButtonAction {
    START,
    STOP,
    RESTART,
};

struct Button {
    ButtonAction action;

    struct ButtonColor {
        CHyprColor foreground;
        CHyprColor background;
    } color;

    float                size     = 0;
    std::string          icon     = "";
    CBox                 hitbox   = {0, 0, 0, 0};
    bool                 visible  = true;
    PHLANIMVAR<float>    opacity  = nullptr;
    PHLANIMVAR<Vector2D> position = nullptr;
};

struct TitleAreaLayout {
    CBox container;
    CBox title;
};

class HyprmodoroDecoration : public IHyprWindowDecoration {
  public:
    HyprmodoroDecoration(PHLWINDOW);

    virtual ~HyprmodoroDecoration();
    virtual SDecorationPositioningInfo getPositioningInfo();
    virtual void                       onPositioningReply(const SDecorationPositioningReply& reply);
    virtual void                       draw(PHLMONITOR, float const& a);
    virtual eDecorationType            getDecorationType();
    virtual void                       updateWindow(PHLWINDOW);
    virtual void                       damageEntire();
    virtual uint64_t                   getDecorationFlags();
    virtual eDecorationLayer           getDecorationLayer();
    virtual std::string                getDisplayName();

    WP<HyprmodoroDecoration>           m_self;

  private:
    CBox                           assignedBoxGlobal();
    void                           setupButtons();
    void                           drawPass(PHLMONITOR, float const& a);
    void                           renderProgressBorder(CBox windowBox, PHLMONITOR pMonitor, float alpha);
    void                           renderTitleBar(CBox windowBox, PHLMONITOR pMonitor, float alpha);
    void                           renderButtons(cairo_t* cairo, const Vector2D& buffer, const float& scale);
    void                           renderTimer(cairo_t* cairo, const Vector2D& buffer, const float& scale);
    void                           updateHoverOffset();
    Vector2D                       cursorRelativeToContainer();
    bool                           isHoveringTitle(const CBox& windowBox, const float& scale);
    bool                           isValidInput();
    void                           onMouseDown(Event::SCallbackInfo& info, IPointer::SButtonEvent e);
    void                           handleButtonClick(ButtonAction buttonAction);

    CBox                           m_bAssignedGeometry;
    PHLWINDOWREF                   m_pWindow;
    SP<CTexture>                   m_pTitleTex;
    SP<CTexture>                   m_pProgressTex;

    TitleAreaLayout                m_layout;
    std::map<ButtonAction, Button> m_vButtons;
    float                          m_reservedYOffset = 0.0f;

    bool                           m_isNearContainer = false;

    PHLANIMVAR<float>              m_hoverOffset = nullptr;
    PHLANIMVAR<float>              m_textOpacity = nullptr;

    CHyprSignalListener            m_pMouseButtonCallback;
    friend class HyprmodoroPassElement;
};
