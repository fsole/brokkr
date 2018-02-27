/*
* Brokkr framework
*
* Copyright(c) 2017 by Ferran Sole
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files(the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions :
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#include "window.h"
#include <cassert>
using namespace bkk;
using namespace bkk::window;

#ifdef WIN32

static window::key_e KeyFromKeyCode(WPARAM keycode)
{
  switch (keycode)
  {
    case 0x57:
      return KEY_UP;
    case 0x41:
      return KEY_LEFT;
    case 0x53:
      return KEY_DOWN;
    case 0x44:
      return KEY_RIGHT;
    case 0x26:
      return KEY_W;
    case 0x25:
      return KEY_A;
    case 0x28:
      return KEY_S;
    case 0x27:
      return KEY_D;
    case 0x47:
      return KEY_G;
    case 0x50:
      return KEY_P;
    case 0x5A:
      return KEY_Z;
    case 0x58:
      return KEY_X;

    case 48:
      return KEY_0;
    case 49:
      return KEY_1;
    case 50:
      return KEY_2;
    case 51:
      return KEY_3;
    case 52:
      return KEY_4;
    case 53:
      return KEY_5;
    case 54:
      return KEY_6;
    case 55:
      return KEY_7;
    case 56:
      return KEY_8;
    case 57:
      return KEY_9;

    default:
      //std::cout << keycode << std::endl;
      break;
  }

  return KEY_COUNT;
}

static long __stdcall WindowProcedure(HWND hWnd, unsigned int msg, WPARAM wp, LPARAM lp)
{
  window_t* window = (window_t*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
  switch (msg)
  {
    case WM_DESTROY:
    {
      window->activeEvent_ = &window->quitEvent_;
      PostQuitMessage(0);
      return 0;
    }
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    {
      window->mouseButtonEvent_ = event_mouse_button_t(MOUSE_LEFT, LOWORD(lp), HIWORD(lp), (msg == WM_LBUTTONDOWN) );
      window->activeEvent_ = &window->mouseButtonEvent_;
      break;
    }

    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    {
      window->mouseButtonEvent_ = event_mouse_button_t(MOUSE_RIGHT, LOWORD(lp), HIWORD(lp), (msg == WM_RBUTTONDOWN));
      window->activeEvent_ = &window->mouseButtonEvent_;
      break;
    }

    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    {
      window->mouseButtonEvent_ = event_mouse_button_t(MOUSE_MIDDLE, LOWORD(lp), HIWORD(lp), (msg == WM_MBUTTONDOWN));
      window->activeEvent_ = &window->mouseButtonEvent_;
      break;
    }

    case WM_MOUSEMOVE:
    {
      window->mouseMoveEvent_.x_ = LOWORD(lp);
      window->mouseMoveEvent_.y_ = HIWORD(lp);
      window->activeEvent_ = &window->mouseMoveEvent_;
      break;
    }
    case WM_KEYDOWN:
    {
      window->keyEvent_.keyCode_ = KeyFromKeyCode( wp );
      window->keyEvent_.pressed_ = true;
      window->activeEvent_ = &window->keyEvent_;
      break;
    }
    case WM_KEYUP:
    {
      window->keyEvent_.keyCode_ = KeyFromKeyCode(wp);;
      window->keyEvent_.pressed_ = false;
      window->activeEvent_ = &window->keyEvent_;
      break;
    }
    case WM_SIZE:
    {
      window->resizeEvent_.width_ = LOWORD(lp);
      window->resizeEvent_.height_ = HIWORD(lp);
      window->activeEvent_ = &window->resizeEvent_;
    }
    default:
      return (long)DefWindowProc(hWnd, msg, wp, lp);
  }

  return 0;
}

void window::create(const char* title, unsigned int width, unsigned int height, window_t* window)
{
  window->instance_ = GetModuleHandle(0);
  window->width_ = width;
  window->height_ = height;
  window->activeEvent_ = nullptr;
  window->title_ = title;

  WNDCLASSEX wndClass;
  wndClass.cbSize = sizeof(WNDCLASSEX);
  wndClass.style = CS_HREDRAW | CS_VREDRAW;
  wndClass.lpfnWndProc = (WNDPROC)WindowProcedure;
  wndClass.cbClsExtra = 0;
  wndClass.cbWndExtra = 0;
  wndClass.hInstance = window->instance_;
  wndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
  wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
  wndClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
  wndClass.lpszMenuName = NULL;
  wndClass.lpszClassName = title;
  wndClass.hIconSm = LoadIcon(NULL, IDI_WINLOGO);
  RegisterClassEx(&wndClass);
  DWORD dwExStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
  DWORD dwStyle = WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;

  RECT windowRect;
  windowRect.left = 0L;
  windowRect.top = 0L;
  windowRect.right = (long)width;
  windowRect.bottom = (long)height;

  AdjustWindowRectEx(&windowRect, dwStyle, FALSE, dwExStyle);
  window->handle_ = CreateWindowEx(0, title, title,
    dwStyle | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
    0, 0, windowRect.right - windowRect.left, windowRect.bottom - windowRect.top,
    NULL, NULL, window->instance_, NULL);

  assert(window->handle_);

  SetWindowLongPtr(window->handle_, GWLP_USERDATA, (LONG_PTR)window);

  ShowWindow(window->handle_, SW_SHOW);
  SetForegroundWindow(window->handle_);
  SetFocus(window->handle_);
}

event_t* window::getNextEvent(window_t* window)
{
  window->activeEvent_ = nullptr;
  MSG message;
  if (PeekMessage(&message, NULL, 0, 0, PM_REMOVE))
  {
    if (message.hwnd == window->handle_)
    {
      DispatchMessage(&message);
    }
  }

  return window->activeEvent_;
}

void window::setTitle(const char* title, window_t* window )
{
  if (SetWindowText(window->handle_, title))
  {
    window->title_ = title;
  }
}


void window::destroy(window_t* window)
{
  DestroyWindow(window->handle_);
}

#else

static window::key_e KeyFromKeyCode(xcb_keycode_t keycode)
{
  switch (keycode)
  {
  case 111:
    return KEY_UP;
  case 116:
    return KEY_DOWN;
  case 114:
    return KEY_RIGHT;
  case 113:
    return KEY_LEFT;
  case 25:
    return KEY_W;
  case 38:
    return KEY_A;
  case 39:
    return KEY_S;
  case 40:
    return KEY_D;
  case 52:
    return KEY_Z;
  case 53:
    return KEY_X;
  }

  return KEY_COUNT;
}

void window::create(const char* title, unsigned int width, unsigned int height, window_t* window)
{
  const xcb_setup_t *setup;
  xcb_screen_iterator_t iter;
  int scr;

  window->connection_ = xcb_connect(NULL, &scr);
  assert(window->connection_);

  window->width_ = width;
  window->height_ = height;

  setup = xcb_get_setup(window->connection_);
  iter = xcb_setup_roots_iterator(setup);
  while (scr-- > 0)
  {
    xcb_screen_next(&iter);
  }

  window->screen_ = iter.data;
  uint32_t value_mask, value_list[32];
  window->handle_ = xcb_generate_id(window->connection_);

  value_mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
  value_list[0] = window->screen_->black_pixel;
  value_list[1] = XCB_EVENT_MASK_KEY_RELEASE |
    XCB_EVENT_MASK_KEY_PRESS |
    XCB_EVENT_MASK_EXPOSURE |
    XCB_EVENT_MASK_STRUCTURE_NOTIFY |
    XCB_EVENT_MASK_POINTER_MOTION |
    XCB_EVENT_MASK_BUTTON_PRESS |
    XCB_EVENT_MASK_BUTTON_RELEASE;

  xcb_create_window(window->connection_,
    XCB_COPY_FROM_PARENT,
    window->handle_, window->screen_->root,
    0, 0, width, height, 0,
    XCB_WINDOW_CLASS_INPUT_OUTPUT,
    window->screen_->root_visual,
    value_mask, value_list);

  xcb_intern_atom_cookie_t cookie = xcb_intern_atom(window->connection_, 1, 12, "WM_PROTOCOLS");
  xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(window->connection_, cookie, 0);

  xcb_intern_atom_cookie_t cookie2 = xcb_intern_atom(window->connection_, 0, 16, "WM_DELETE_WINDOW");
  window->atomWmDeleteWindow_ = xcb_intern_atom_reply(window->connection_, cookie2, 0);

  xcb_change_property(window->connection_, XCB_PROP_MODE_REPLACE,
    window->handle_, (*reply).atom, 4, 32, 1,
    &(*window->atomWmDeleteWindow_).atom);

  xcb_change_property(window->connection_, XCB_PROP_MODE_REPLACE,
    window->handle_, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8,
    strlen(title), title);

  free(reply);

  xcb_map_window(window->connection_, window->handle_);
}

event_t* window::GetNextEvent(window_t* window)
{
  event_t* result = nullptr;
  xcb_flush(window->connection_);
  xcb_generic_event_t *event = 0;
  if ((event = xcb_poll_for_event(window->connection_)))
  {
    switch (event->response_type & 0x7f)
    {
    case XCB_CLIENT_MESSAGE:
    {
      if ((*(xcb_client_message_event_t*)event).data.data32[0] == (*window->atomWmDeleteWindow_).atom)
      {
        result = &window->quitEvent_;
      }
      break;
    }
    case XCB_CONFIGURE_NOTIFY:
    {
      xcb_configure_notify_event_t* configEvent = (xcb_configure_notify_event_t*)event;
      uint32_t width = configEvent->width;
      uint32_t height = configEvent->height;
      if (width != window->width_ || height != window->height_)
      {
        window->width_ = width;
        window->height_ = height;

        window->resizeEvent_.width_ = width;
        window->resizeEvent_.height_ = height;
        result = &window->resizeEvent_;
      }
      break;
    }
    case XCB_MOTION_NOTIFY:
    {
      xcb_motion_notify_event_t* move = (xcb_motion_notify_event_t*)event;
      window->mouseMoveEvent_.x_ = move->event_x;
      window->mouseMoveEvent_.y_ = move->event_y;
      result = &window->mouseMoveEvent_;
      break;
    }

    case XCB_BUTTON_PRESS:
    case XCB_BUTTON_RELEASE:
    {
      xcb_button_press_event_t* buttonPress = (xcb_button_press_event_t*)event;
      mouse_button_e button = MOUSE_LEFT;

      if (buttonPress->detail == XCB_BUTTON_INDEX_3)
      {
        button = MOUSE_RIGHT;
      }
      else if (buttonPress->detail == XCB_BUTTON_INDEX_2)
      {
        button = MOUSE_MIDDLE;
      }
      window->mouseButtonEvent_.x_ = buttonPress->event_x;
      window->mouseButtonEvent_.y_ = buttonPress->event_y;
      window->mouseButtonEvent_.button_ = button;
      window->mouseButtonEvent_.pressed_ = (event->response_type & 0x7f) == XCB_BUTTON_PRESS;
      result = &window->mouseButtonEvent_;
      break;
    }
    case XCB_KEY_PRESS:
    {
      const xcb_key_press_event_t* keyEvent = (const xcb_key_press_event_t*)event;
      window->keyEvent_.keyCode_ = KeyFromKeyCode(keyEvent->detail);
      window->keyEvent_.pressed_ = true;
      result = &window->keyEvent_;
      break;
    }
    case XCB_KEY_RELEASE:
    {
      const xcb_key_release_event_t* keyEvent = (const xcb_key_release_event_t*)event;
      window->keyEvent_.keyCode_ = KeyFromKeyCode(keyEvent->detail);
      window->keyEvent_.pressed_ = false;
      result = &window->keyEvent_;
      break;
    }
    default:
    {
      result = &window->unknowEvent_;
    }
    }
    free(event);
  }

  return result;
}

void window::destroy(window_t* window)
{
  free(window->atomWmDeleteWindow_);
  xcb_unmap_window(window->connection_, window->handle_);
  xcb_destroy_window(window->connection_, window->handle_);
  xcb_disconnect(window->connection_);
}

#endif
