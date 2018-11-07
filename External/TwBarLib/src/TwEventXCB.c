//  ---------------------------------------------------------------------------
//
//  @file       TwEventXCB.c
//  @brief      Helper: 
//              translate and forward mouse and keyboard events 
//              from XCB to AntTweakBar
//  
//  @contrib    Egor Yusov
//
//  ---------------------------------------------------------------------------

#include <stdlib.h>
#include <xcb/xcb.h>
#include "xcb_keysyms/xcb_keysyms.h"
#include <AntTweakBar.h>
#include <X11/keysym.h>

static int s_KMod = 0;

static xcb_key_symbols_t* g_syms = NULL;

TW_API int TW_CDECL_CALL TwInitXCBKeysms(void *connection)
{
    g_syms = xcb_key_symbols_alloc((xcb_connection_t*)connection);
}

TW_API int TW_CDECL_CALL TwReleaseXCBKeysyms()
{
    if (g_syms)
    {
        xcb_key_symbols_free(g_syms);
    }
}

// ----------------------------------------------------------------------
// ----------------------------------------------------------------------
static int _XCBKeyRelease(xcb_key_release_event_t* event)
{
    xcb_keysym_t keysym = xcb_key_press_lookup_keysym(g_syms, event, 0);
    switch (keysym)
    {
    case XK_Control_L:
    case XK_Control_R: s_KMod &= ~TW_KMOD_CTRL;   break;

    case XK_Shift_L:
    case XK_Shift_R:   s_KMod &= ~TW_KMOD_SHIFT;  break;

    case XK_Alt_L:
    case XK_Alt_R:     s_KMod &= ~TW_KMOD_ALT;    break;
    }
    return 0;
}

// ----------------------------------------------------------------------
// ----------------------------------------------------------------------
static int _XCBKeyPress(xcb_key_press_event_t* event)
{
    int modifiers = 0;  // modifiers sent to AntTweakBar
    if (event->state & XCB_KEY_BUT_MASK_CONTROL)
        modifiers |= TW_KMOD_CTRL;
    if (event->state & XCB_KEY_BUT_MASK_SHIFT)
        modifiers |= TW_KMOD_SHIFT;
    if (event->state & XCB_KEY_BUT_MASK_MOD_1)
        modifiers |= TW_KMOD_ALT;

    int k = 0;          // key sent to AntTweakBar
    switch (event->detail)
    {
        case 0x09:  k = TW_KEY_ESCAPE;    break;
        case 0x43:  k = TW_KEY_F1;        break;
        case 0x44:  k = TW_KEY_F2;        break;
        case 0x45:  k = TW_KEY_F3;        break;
        case 0x46:  k = TW_KEY_F4;        break;
        case 0x47:  k = TW_KEY_F5;        break;
        case 0x48:  k = TW_KEY_F6;        break;
        case 0x49:  k = TW_KEY_F7;        break;
        case 0x4A:  k = TW_KEY_F8;        break;
        case 0x4B:  k = TW_KEY_F9;        break;
        case 0x4C:  k = TW_KEY_F10;       break;
        case 0x5F:  k = TW_KEY_F11;       break;
        case 0x60:  k = TW_KEY_F12;       break;
        case 0x6F:  k = TW_KEY_UP;        break;
        case 0x74:  k = TW_KEY_DOWN;      break;
        case 0x72:  k = TW_KEY_RIGHT;     break;
        case 0x71:  k = TW_KEY_LEFT;      break;
        case 0x24:  k = TW_KEY_RETURN;    break;
        case 0x76:  k = TW_KEY_INSERT;    break;
        case 0x77:  k = TW_KEY_DELETE;    break;
        case 0x16:  k = TW_KEY_BACKSPACE; break;
        case 0x6E:  k = TW_KEY_HOME;      break;
        case 0x17:  k = TW_KEY_TAB;       break;
        case 0x73:  k = TW_KEY_END;       break;
        case 0x68:  k = TW_KEY_RETURN;    break;
        case 0x70:  k = TW_KEY_PAGE_UP;   break;
        case 0x75:  k = TW_KEY_PAGE_DOWN; break;
    }      

    if (k==0)
    {
        xcb_keysym_t keysym = xcb_key_press_lookup_keysym(g_syms, event, 0);
        switch (keysym)
        {
            case XK_Control_L:
            case XK_Control_R: s_KMod |= TW_KMOD_CTRL;  break;

            case XK_Shift_L:
            case XK_Shift_R:   s_KMod |= TW_KMOD_SHIFT; break;

            case XK_Alt_L:
            case XK_Alt_R:     s_KMod |= TW_KMOD_ALT;   break;

#ifdef XK_Enter
            case XK_Enter:     k = TW_KEY_RETURN;    break;
#endif

#ifdef XK_KP_Home
            case XK_KP_Home:   k = TW_KEY_HOME;      break;
            case XK_KP_End:    k = TW_KEY_END;       break;
            case XK_KP_Delete: k = TW_KEY_DELETE;    break;
#endif

#ifdef XK_KP_Up
            case XK_KP_Up:     k = TW_KEY_UP;        break;
            case XK_KP_Down:   k = TW_KEY_DOWN;      break;
            case XK_KP_Right:  k = TW_KEY_RIGHT;     break;
            case XK_KP_Left:   k = TW_KEY_LEFT;      break;
#endif

#ifdef XK_KP_Page_Up
            case XK_KP_Page_Up:   k = TW_KEY_PAGE_UP;    break;
            case XK_KP_Page_Down: k = TW_KEY_PAGE_DOWN;  break;
#endif

#ifdef XK_KP_Tab
            case XK_KP_Tab:    k = TW_KEY_TAB;       break;
#endif

            default:
                // should we do that, or rely on the buffer (see code below)
                if (keysym > 12 && keysym < 127) 
                    k = keysym;
            break;
        }  
    }

#if 0        
    if (k == 0 && num_char)
    {
        int i, handled = 0;
        for (i=0; i<num_char; ++i)
            if (TwKeyPressed(buffer[i], modifiers))
                handled = 1;
        return handled;
    }
#endif

    // if we have a valid key, send to AntTweakBar
    // -------------------------------------------
    return (k > 0) ? TwKeyPressed(k, modifiers) : 0;
}


// ----------------------------------------------------------------------
// ----------------------------------------------------------------------
TW_API int TW_CDECL_CALL TwEventXCB(void *xcbEvent)
{
    xcb_generic_event_t* event = (xcb_generic_event_t *)xcbEvent;
    
    switch (event->response_type & 0x7f)
    {
        case XCB_MOTION_NOTIFY:
        {
            xcb_motion_notify_event_t *motion = (xcb_motion_notify_event_t *)event;
            return TwMouseMotion((int)motion->event_x, (int)motion->event_y);
        }
        break;

        case XCB_BUTTON_PRESS:
        {
            xcb_button_press_event_t *press = (xcb_button_press_event_t *)event;
            TwMouseButtonID ButtonId = TW_MOUSE_LEFT;
            if (press->detail == XCB_BUTTON_INDEX_1)
                ButtonId = TW_MOUSE_LEFT;
            if (press->detail == XCB_BUTTON_INDEX_2)
                ButtonId = TW_MOUSE_MIDDLE;
            if (press->detail == XCB_BUTTON_INDEX_3)
                ButtonId = TW_MOUSE_RIGHT;
            return TwMouseButton(TW_MOUSE_PRESSED, ButtonId);
        }
        break;

        case XCB_BUTTON_RELEASE:
        {
            xcb_button_release_event_t *press = (xcb_button_release_event_t *)event;
            TwMouseButtonID ButtonId = TW_MOUSE_LEFT;
            if (press->detail == XCB_BUTTON_INDEX_1)
                ButtonId = TW_MOUSE_LEFT;
            if (press->detail == XCB_BUTTON_INDEX_2)
                ButtonId = TW_MOUSE_MIDDLE;
            if (press->detail == XCB_BUTTON_INDEX_3)
                ButtonId = TW_MOUSE_RIGHT;
            return TwMouseButton(TW_MOUSE_RELEASED, ButtonId);
        }
        break;

        case XCB_KEY_PRESS:
        {
            xcb_key_press_event_t* keyEvent = (xcb_key_press_event_t *)event;
            return _XCBKeyPress(keyEvent);
        }
        break;

        case XCB_KEY_RELEASE:
        {
            xcb_key_release_event_t* keyEvent = (xcb_key_release_event_t *)event;
            return _XCBKeyRelease(keyEvent);
        }
        break;

        case XCB_CONFIGURE_NOTIFY:
        {
            const xcb_configure_notify_event_t *cfgEvent = (const xcb_configure_notify_event_t *)event;
            TwWindowSize(cfgEvent->width, cfgEvent->height);
            return 0;
        }
        break;

        default:
            break;
    }

    return 0;
}
