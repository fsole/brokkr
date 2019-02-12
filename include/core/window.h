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

#ifndef WINDOW_H
#define WINDOW_H

#include <stdint.h>

#ifdef WIN32
#define NOMINMAX
#define _USE_MATH_DEFINES
#include <windows.h>
#else
#include <xcb/xcb.h>
#endif

namespace bkk
{
  namespace core
  {
    namespace window
    {
      enum key_e
      {
        KEY_UP = 0,
        KEY_DOWN = 1,
        KEY_LEFT = 2,
        KEY_RIGHT = 3,

        KEY_0 = 48,
        KEY_1 = 49,
        KEY_2 = 50,
        KEY_3 = 51,
        KEY_4 = 52,
        KEY_5 = 53,
        KEY_6 = 54,
        KEY_7 = 55,
        KEY_8 = 56,
        KEY_9 = 57,

        KEY_A = 'a',
        KEY_B = 'b',
        KEY_C = 'c',
        KEY_D = 'd',
        KEY_E = 'e',
        KEY_F = 'f',
        KEY_G = 'g',
        KEY_H = 'h',
        KEY_I = 'i',
        KEY_J = 'j',
        KEY_K = 'k',
        KEY_L = 'l',
        KEY_M = 'm',
        KEY_N = 'n',
        KEY_O = 'o',
        KEY_P = 'p',
        KEY_Q = 'q',
        KEY_R = 'r',
        KEY_S = 's',
        KEY_T = 't',
        KEY_U = 'u',
        KEY_V = 'v',
        KEY_W = 'w',
        KEY_X = 'x',
        KEY_Y = 'y',
        KEY_Z = 'z',

        KEY_UNDEFINED = -1
      };

      enum mouse_button_e
      {
        MOUSE_LEFT = 0,
        MOUSE_RIGHT = 1,
        MOUSE_MIDDLE = 2,
      };

      enum event_type_e
      {
        EVENT_QUIT,
        EVENT_RESIZE,
        EVENT_KEY,
        EVENT_MOUSE_MOVE,
        EVENT_MOUSE_BUTTON,
        EVENT_UNKNOW
      };

      struct event_t
      {
        event_t(event_type_e type):type(type) {}
        event_type_e type;
      };

      struct event_unknow_t : public event_t
      {
        event_unknow_t():event_t(EVENT_UNKNOW) {}
      };

      struct event_quit_t : public event_t
      {
        event_quit_t():event_t(EVENT_QUIT) {}
      };

      struct event_resize_t : public event_t
      {
        event_resize_t():event_t(EVENT_RESIZE) {};
        uint32_t width;
        uint32_t height;
      };

      struct event_key_t : public event_t
      {
        event_key_t():event_t(EVENT_KEY) {};
        key_e keyCode;
        bool pressed;
      };

      struct event_mouse_move_t : public event_t
      {
        event_mouse_move_t():event_t(EVENT_MOUSE_MOVE) {};
        uint32_t x;
        uint32_t y;
      };

      struct event_mouse_button_t : public event_t
      {
        event_mouse_button_t() :event_t(EVENT_MOUSE_BUTTON) {};
        event_mouse_button_t(mouse_button_e button, uint32_t x, uint32_t y, bool pressed)
        :event_t(EVENT_MOUSE_BUTTON),
         button(button),
         x(x),
         y(y),
         pressed(pressed) {};

        mouse_button_e button;
        uint32_t x;
        uint32_t y;
        bool pressed;
      };

      struct window_t
      {
        uint32_t width;
        uint32_t height;
        char title[128];

#ifdef WIN32
        HINSTANCE instance;
        HWND handle;
        event_t* activeEvent;
#else
        xcb_connection_t* connection_ = nullptr;
        xcb_screen_t* screen_ = nullptr;
        xcb_window_t handle_;
        xcb_intern_atom_reply_t* atomWmDeleteWindow_ = nullptr;
#endif

        //Events
        event_quit_t quitEvent;
        event_resize_t resizeEvent;
        event_key_t keyEvent;
        event_mouse_move_t mouseMoveEvent;
        event_mouse_button_t mouseButtonEvent;
        event_unknow_t unknowEvent;
      };

      void create(const char* title, unsigned int width, unsigned int height, window_t* window);
      void setTitle(const char* title, window_t* window);
      event_t* getNextEvent(window_t* window);
      void destroy(window_t* window);

    } //window 
  }//core
}//bkk
#endif // WINDOW_H