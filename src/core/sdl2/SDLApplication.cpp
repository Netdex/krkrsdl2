
#include "tjsCommHead.h"
#include "WindowImpl.h"
#ifndef _WIN32
#include "VirtualKey.h"
#endif
#include "Application.h"
#include "SystemImpl.h"
#include "TVPWindow.h"
#include "SysInitIntf.h"
#include "CharacterSet.h"
#include "WaveImpl.h"
#include "TimerThread.h"
#include "MsgIntf.h"
#include "DebugIntf.h"
#include "tjsArray.h"
#include "StorageIntf.h"
#include "SDLBitmapCompletion.h"
#include "ScriptMgnIntf.h"
#ifdef KRKRZ_ENABLE_CANVAS
#include "OpenGLScreenSDL2.h"
#endif
#include <SDL.h>
#ifdef USE_SDL_MAIN
#include <SDL_main.h>
#endif

#include <unistd.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

#if defined(__IPHONEOS__) || defined(__ANDROID__) || defined(__EMSCRIPTEN__)
#define KRKRSDL2_WINDOW_SIZE_IS_LAYER_SIZE
#endif

extern void TVPLoadMessage();

class TVPWindowLayer;
static TVPWindowLayer *_lastWindowLayer, *_currentWindowLayer;
static SDL_GameController** sdl_controllers = NULL;
static int sdl_controller_num = 0;

#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
static void process_events();
#else
static bool process_events();
#endif

static void refresh_controllers()
{
#if defined(__IPHONEOS__)
	// For some reason, invalid pointers get set
	{
		return;
	}
#endif
	if (SDL_WasInit(SDL_INIT_GAMECONTROLLER) == 0)
	{
		SDL_Init(SDL_INIT_GAMECONTROLLER);
	}
	if (sdl_controller_num != 0 && sdl_controllers != NULL)
	{
		for (int i = 0; i < sdl_controller_num; i += 1)
		{
			if (sdl_controllers[i] != NULL)
			{
				SDL_GameControllerClose(sdl_controllers[i]);
				sdl_controllers[i] = NULL;
			}
		}
		sdl_controller_num = 0;
		free(sdl_controllers);
		sdl_controllers = NULL;
	}
	sdl_controller_num = SDL_NumJoysticks();
	sdl_controllers = (SDL_GameController**)malloc(sizeof(SDL_GameController*) * sdl_controller_num);
	if (!sdl_controllers)
	{
		sdl_controller_num = 0;
		TVPAddLog(ttstr("Could not allocate SDL controller memory"));
		return;
	}
	for (int i = 0; i < sdl_controller_num; i += 1)
	{
		if (SDL_IsGameController(i))
		{
			sdl_controllers[i] = SDL_GameControllerOpen(i);
			if (!sdl_controllers[i])
			{
				TVPAddLog(ttstr("Could not open controller: ") + ttstr(SDL_GetError()));
			}
		}
	}
}

static Uint8 vk_key_to_sdl_gamecontrollerbutton(tjs_uint key)
{
	if (key == VK_PAD1) return SDL_CONTROLLER_BUTTON_A;
	if (key == VK_PAD2) return SDL_CONTROLLER_BUTTON_B;
	if (key == VK_PAD3) return SDL_CONTROLLER_BUTTON_X;
	if (key == VK_PAD4) return SDL_CONTROLLER_BUTTON_Y;
	if (key == VK_PAD7) return SDL_CONTROLLER_BUTTON_BACK;
	if (key == VK_PAD8) return SDL_CONTROLLER_BUTTON_START;
	if (key == VK_PAD9) return SDL_CONTROLLER_BUTTON_LEFTSTICK;
	if (key == VK_PAD10) return SDL_CONTROLLER_BUTTON_RIGHTSTICK;
	if (key == VK_PAD5) return SDL_CONTROLLER_BUTTON_LEFTSHOULDER;
	if (key == VK_PAD6) return SDL_CONTROLLER_BUTTON_RIGHTSHOULDER;
	if (key == VK_PADUP) return SDL_CONTROLLER_BUTTON_DPAD_UP;
	if (key == VK_PADDOWN) return SDL_CONTROLLER_BUTTON_DPAD_DOWN;
	if (key == VK_PADLEFT) return SDL_CONTROLLER_BUTTON_DPAD_LEFT;
	if (key == VK_PADRIGHT) return SDL_CONTROLLER_BUTTON_DPAD_RIGHT;
	return 0;
}

static tjs_uint sdl_gamecontrollerbutton_to_vk_key(Uint8 key)
{
	if (key == SDL_CONTROLLER_BUTTON_A) return VK_PAD1;
	if (key == SDL_CONTROLLER_BUTTON_B) return VK_PAD2;
	if (key == SDL_CONTROLLER_BUTTON_X) return VK_PAD3;
	if (key == SDL_CONTROLLER_BUTTON_Y) return VK_PAD4;
	if (key == SDL_CONTROLLER_BUTTON_BACK) return VK_PAD7;
	if (key == SDL_CONTROLLER_BUTTON_START) return VK_PAD8;
	if (key == SDL_CONTROLLER_BUTTON_LEFTSTICK) return VK_PAD9;
	if (key == SDL_CONTROLLER_BUTTON_RIGHTSTICK) return VK_PAD10;
	if (key == SDL_CONTROLLER_BUTTON_LEFTSHOULDER) return VK_PAD5;
	if (key == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) return VK_PAD6;
	if (key == SDL_CONTROLLER_BUTTON_DPAD_UP) return VK_PADUP;
	if (key == SDL_CONTROLLER_BUTTON_DPAD_DOWN) return VK_PADDOWN;
	if (key == SDL_CONTROLLER_BUTTON_DPAD_LEFT) return VK_PADLEFT;
	if (key == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) return VK_PADRIGHT;
	return 0;
}

#define MK_SHIFT 4
#define MK_CONTROL 8
#define MK_ALT (0x20)

static SDL_Cursor* sdl_system_cursors[SDL_NUM_SYSTEM_CURSORS] = {0};

static SDL_Keycode vk_key_to_sdl_key(tjs_uint key)
{
	// This is generated using regex find replace
	if (key == VK_BACK) return SDLK_BACKSPACE;
	if (key == VK_TAB) return SDLK_TAB;
	if (key == VK_CLEAR) return SDLK_CLEAR;
	if (key == VK_RETURN) return SDLK_RETURN;
	if (key == VK_PAUSE) return SDLK_PAUSE;
	if (key == VK_CAPITAL) return SDLK_CAPSLOCK;
	if (key == VK_ESCAPE) return SDLK_ESCAPE;
	if (key == VK_SPACE) return SDLK_SPACE;
	if (key == VK_PRIOR) return SDLK_PAGEUP;
	if (key == VK_NEXT) return SDLK_PAGEDOWN;
	if (key == VK_END) return SDLK_END;
	if (key == VK_HOME) return SDLK_HOME;
	if (key == VK_LEFT) return SDLK_LEFT;
	if (key == VK_UP) return SDLK_UP;
	if (key == VK_RIGHT) return SDLK_RIGHT;
	if (key == VK_DOWN) return SDLK_DOWN;
	if (key == VK_SELECT) return SDLK_SELECT;
	if (key == VK_EXECUTE) return SDLK_EXECUTE;
	if (key == VK_SNAPSHOT) return SDLK_PRINTSCREEN;
	if (key == VK_INSERT) return SDLK_INSERT;
	if (key == VK_DELETE) return SDLK_DELETE;
	if (key == VK_HELP) return SDLK_HELP;
	if (key == VK_0) return SDLK_0;
	if (key == VK_1) return SDLK_1;
	if (key == VK_2) return SDLK_2;
	if (key == VK_3) return SDLK_3;
	if (key == VK_4) return SDLK_4;
	if (key == VK_5) return SDLK_5;
	if (key == VK_6) return SDLK_6;
	if (key == VK_7) return SDLK_7;
	if (key == VK_8) return SDLK_8;
	if (key == VK_9) return SDLK_9;
	if (key == VK_A) return SDLK_a;
	if (key == VK_B) return SDLK_b;
	if (key == VK_C) return SDLK_c;
	if (key == VK_D) return SDLK_d;
	if (key == VK_E) return SDLK_e;
	if (key == VK_F) return SDLK_f;
	if (key == VK_G) return SDLK_g;
	if (key == VK_H) return SDLK_h;
	if (key == VK_I) return SDLK_i;
	if (key == VK_J) return SDLK_j;
	if (key == VK_K) return SDLK_k;
	if (key == VK_L) return SDLK_l;
	if (key == VK_M) return SDLK_m;
	if (key == VK_N) return SDLK_n;
	if (key == VK_O) return SDLK_o;
	if (key == VK_P) return SDLK_p;
	if (key == VK_Q) return SDLK_q;
	if (key == VK_R) return SDLK_r;
	if (key == VK_S) return SDLK_s;
	if (key == VK_T) return SDLK_t;
	if (key == VK_U) return SDLK_u;
	if (key == VK_V) return SDLK_v;
	if (key == VK_W) return SDLK_w;
	if (key == VK_X) return SDLK_x;
	if (key == VK_Y) return SDLK_y;
	if (key == VK_Z) return SDLK_z;
	if (key == VK_LWIN) return SDLK_LGUI;
	if (key == VK_RWIN) return SDLK_RGUI;
	if (key == VK_SLEEP) return SDLK_SLEEP;
	if (key == VK_NUMPAD0) return SDLK_KP_0;
	if (key == VK_NUMPAD1) return SDLK_KP_1;
	if (key == VK_NUMPAD2) return SDLK_KP_2;
	if (key == VK_NUMPAD3) return SDLK_KP_3;
	if (key == VK_NUMPAD4) return SDLK_KP_4;
	if (key == VK_NUMPAD5) return SDLK_KP_5;
	if (key == VK_NUMPAD6) return SDLK_KP_6;
	if (key == VK_NUMPAD7) return SDLK_KP_7;
	if (key == VK_NUMPAD8) return SDLK_KP_8;
	if (key == VK_NUMPAD9) return SDLK_KP_9;
	if (key == VK_MULTIPLY) return SDLK_KP_MULTIPLY;
	if (key == VK_ADD) return SDLK_KP_PLUS;
	if (key == VK_SUBTRACT) return SDLK_KP_MINUS;
	if (key == VK_DECIMAL) return SDLK_KP_PERIOD;
	if (key == VK_DIVIDE) return SDLK_KP_DIVIDE;
	if (key == VK_F1) return SDLK_F1;
	if (key == VK_F2) return SDLK_F2;
	if (key == VK_F3) return SDLK_F3;
	if (key == VK_F4) return SDLK_F4;
	if (key == VK_F5) return SDLK_F5;
	if (key == VK_F6) return SDLK_F6;
	if (key == VK_F7) return SDLK_F7;
	if (key == VK_F8) return SDLK_F8;
	if (key == VK_F9) return SDLK_F9;
	if (key == VK_F10) return SDLK_F10;
	if (key == VK_F11) return SDLK_F11;
	if (key == VK_F12) return SDLK_F12;
	if (key == VK_F13) return SDLK_F13;
	if (key == VK_F14) return SDLK_F14;
	if (key == VK_F15) return SDLK_F15;
	if (key == VK_F16) return SDLK_F16;
	if (key == VK_F17) return SDLK_F17;
	if (key == VK_F18) return SDLK_F18;
	if (key == VK_F19) return SDLK_F19;
	if (key == VK_F20) return SDLK_F20;
	if (key == VK_F21) return SDLK_F21;
	if (key == VK_F22) return SDLK_F22;
	if (key == VK_F23) return SDLK_F23;
	if (key == VK_F24) return SDLK_F24;
	if (key == VK_NUMLOCK) return SDLK_NUMLOCKCLEAR;
	if (key == VK_SCROLL) return SDLK_SCROLLLOCK;
	if (key == VK_LSHIFT) return SDLK_LSHIFT;
	if (key == VK_RSHIFT) return SDLK_RSHIFT;
	if (key == VK_LCONTROL) return SDLK_LCTRL;
	if (key == VK_RCONTROL) return SDLK_RCTRL;
	if (key == VK_LMENU) return SDLK_LALT;
	if (key == VK_RMENU) return SDLK_RALT;
	if (key == VK_BROWSER_BACK) return SDLK_AC_BACK;
	if (key == VK_BROWSER_FORWARD) return SDLK_AC_FORWARD;
	if (key == VK_BROWSER_REFRESH) return SDLK_AC_REFRESH;
	if (key == VK_BROWSER_STOP) return SDLK_AC_STOP;
	if (key == VK_BROWSER_SEARCH) return SDLK_AC_SEARCH;
	if (key == VK_BROWSER_FAVORITES) return SDLK_AC_BOOKMARKS;
	if (key == VK_BROWSER_HOME) return SDLK_AC_HOME;
	if (key == VK_VOLUME_MUTE) return SDLK_MUTE;
	if (key == VK_VOLUME_DOWN) return SDLK_VOLUMEDOWN;
	if (key == VK_VOLUME_UP) return SDLK_VOLUMEUP;
	if (key == VK_MEDIA_NEXT_TRACK) return SDLK_AUDIONEXT;
	if (key == VK_MEDIA_PREV_TRACK) return SDLK_AUDIOPREV;
	if (key == VK_MEDIA_STOP) return SDLK_AUDIOSTOP;
	if (key == VK_MEDIA_PLAY_PAUSE) return SDLK_AUDIOPLAY;
	if (key == VK_LAUNCH_MAIL) return SDLK_MAIL;
	if (key == VK_LAUNCH_MEDIA_SELECT) return SDLK_MEDIASELECT;
	return 0;
}

static tjs_uint sdl_key_to_vk_key(SDL_Keycode key)
{
	// This is generated using regex find replace
	if (key == SDLK_BACKSPACE) return VK_BACK;
	if (key == SDLK_TAB) return VK_TAB;
	if (key == SDLK_CLEAR) return VK_CLEAR;
	if (key == SDLK_RETURN) return VK_RETURN;
	if (key == SDLK_PAUSE) return VK_PAUSE;
	if (key == SDLK_CAPSLOCK) return VK_CAPITAL;
	if (key == SDLK_ESCAPE) return VK_ESCAPE;
	if (key == SDLK_SPACE) return VK_SPACE;
	if (key == SDLK_PAGEUP) return VK_PRIOR;
	if (key == SDLK_PAGEDOWN) return VK_NEXT;
	if (key == SDLK_END) return VK_END;
	if (key == SDLK_HOME) return VK_HOME;
	if (key == SDLK_LEFT) return VK_LEFT;
	if (key == SDLK_UP) return VK_UP;
	if (key == SDLK_RIGHT) return VK_RIGHT;
	if (key == SDLK_DOWN) return VK_DOWN;
	if (key == SDLK_SELECT) return VK_SELECT;
	if (key == SDLK_EXECUTE) return VK_EXECUTE;
	if (key == SDLK_PRINTSCREEN) return VK_SNAPSHOT;
	if (key == SDLK_INSERT) return VK_INSERT;
	if (key == SDLK_DELETE) return VK_DELETE;
	if (key == SDLK_HELP) return VK_HELP;
	if (key == SDLK_0) return VK_0;
	if (key == SDLK_1) return VK_1;
	if (key == SDLK_2) return VK_2;
	if (key == SDLK_3) return VK_3;
	if (key == SDLK_4) return VK_4;
	if (key == SDLK_5) return VK_5;
	if (key == SDLK_6) return VK_6;
	if (key == SDLK_7) return VK_7;
	if (key == SDLK_8) return VK_8;
	if (key == SDLK_9) return VK_9;
	if (key == SDLK_a) return VK_A;
	if (key == SDLK_b) return VK_B;
	if (key == SDLK_c) return VK_C;
	if (key == SDLK_d) return VK_D;
	if (key == SDLK_e) return VK_E;
	if (key == SDLK_f) return VK_F;
	if (key == SDLK_g) return VK_G;
	if (key == SDLK_h) return VK_H;
	if (key == SDLK_i) return VK_I;
	if (key == SDLK_j) return VK_J;
	if (key == SDLK_k) return VK_K;
	if (key == SDLK_l) return VK_L;
	if (key == SDLK_m) return VK_M;
	if (key == SDLK_n) return VK_N;
	if (key == SDLK_o) return VK_O;
	if (key == SDLK_p) return VK_P;
	if (key == SDLK_q) return VK_Q;
	if (key == SDLK_r) return VK_R;
	if (key == SDLK_s) return VK_S;
	if (key == SDLK_t) return VK_T;
	if (key == SDLK_u) return VK_U;
	if (key == SDLK_v) return VK_V;
	if (key == SDLK_w) return VK_W;
	if (key == SDLK_x) return VK_X;
	if (key == SDLK_y) return VK_Y;
	if (key == SDLK_z) return VK_Z;
	if (key == SDLK_LGUI) return VK_LWIN;
	if (key == SDLK_RGUI) return VK_RWIN;
	if (key == SDLK_SLEEP) return VK_SLEEP;
	if (key == SDLK_KP_0) return VK_NUMPAD0;
	if (key == SDLK_KP_1) return VK_NUMPAD1;
	if (key == SDLK_KP_2) return VK_NUMPAD2;
	if (key == SDLK_KP_3) return VK_NUMPAD3;
	if (key == SDLK_KP_4) return VK_NUMPAD4;
	if (key == SDLK_KP_5) return VK_NUMPAD5;
	if (key == SDLK_KP_6) return VK_NUMPAD6;
	if (key == SDLK_KP_7) return VK_NUMPAD7;
	if (key == SDLK_KP_8) return VK_NUMPAD8;
	if (key == SDLK_KP_9) return VK_NUMPAD9;
	if (key == SDLK_KP_MULTIPLY) return VK_MULTIPLY;
	if (key == SDLK_KP_PLUS) return VK_ADD;
	if (key == SDLK_KP_MINUS) return VK_SUBTRACT;
	if (key == SDLK_KP_PERIOD) return VK_DECIMAL;
	if (key == SDLK_KP_DIVIDE) return VK_DIVIDE;
	if (key == SDLK_F1) return VK_F1;
	if (key == SDLK_F2) return VK_F2;
	if (key == SDLK_F3) return VK_F3;
	if (key == SDLK_F4) return VK_F4;
	if (key == SDLK_F5) return VK_F5;
	if (key == SDLK_F6) return VK_F6;
	if (key == SDLK_F7) return VK_F7;
	if (key == SDLK_F8) return VK_F8;
	if (key == SDLK_F9) return VK_F9;
	if (key == SDLK_F10) return VK_F10;
	if (key == SDLK_F11) return VK_F11;
	if (key == SDLK_F12) return VK_F12;
	if (key == SDLK_F13) return VK_F13;
	if (key == SDLK_F14) return VK_F14;
	if (key == SDLK_F15) return VK_F15;
	if (key == SDLK_F16) return VK_F16;
	if (key == SDLK_F17) return VK_F17;
	if (key == SDLK_F18) return VK_F18;
	if (key == SDLK_F19) return VK_F19;
	if (key == SDLK_F20) return VK_F20;
	if (key == SDLK_F21) return VK_F21;
	if (key == SDLK_F22) return VK_F22;
	if (key == SDLK_F23) return VK_F23;
	if (key == SDLK_F24) return VK_F24;
	if (key == SDLK_NUMLOCKCLEAR) return VK_NUMLOCK;
	if (key == SDLK_SCROLLLOCK) return VK_SCROLL;
	if (key == SDLK_LSHIFT) return VK_LSHIFT;
	if (key == SDLK_RSHIFT) return VK_RSHIFT;
	if (key == SDLK_LCTRL) return VK_LCONTROL;
	if (key == SDLK_RCTRL) return VK_RCONTROL;
	if (key == SDLK_MENU) return VK_LMENU;
	if (key == SDLK_MENU) return VK_RMENU;
	if (key == SDLK_LALT) return VK_LMENU;
	if (key == SDLK_RALT) return VK_RMENU;
	if (key == SDLK_AC_BACK) return VK_BROWSER_BACK;
	if (key == SDLK_AC_FORWARD) return VK_BROWSER_FORWARD;
	if (key == SDLK_AC_REFRESH) return VK_BROWSER_REFRESH;
	if (key == SDLK_AC_STOP) return VK_BROWSER_STOP;
	if (key == SDLK_AC_SEARCH) return VK_BROWSER_SEARCH;
	if (key == SDLK_AC_BOOKMARKS) return VK_BROWSER_FAVORITES;
	if (key == SDLK_AC_HOME) return VK_BROWSER_HOME;
	if (key == SDLK_MUTE) return VK_VOLUME_MUTE;
	if (key == SDLK_VOLUMEDOWN) return VK_VOLUME_DOWN;
	if (key == SDLK_VOLUMEUP) return VK_VOLUME_UP;
	if (key == SDLK_AUDIONEXT) return VK_MEDIA_NEXT_TRACK;
	if (key == SDLK_AUDIOPREV) return VK_MEDIA_PREV_TRACK;
	if (key == SDLK_AUDIOSTOP) return VK_MEDIA_STOP;
	if (key == SDLK_AUDIOPLAY) return VK_MEDIA_PLAY_PAUSE;
	if (key == SDLK_MAIL) return VK_LAUNCH_MAIL;
	if (key == SDLK_MEDIASELECT) return VK_LAUNCH_MEDIA_SELECT;
	return 0;
}

static int GetShiftState() {
	int s = 0;
	if(TVPGetAsyncKeyState(VK_MENU)) s |= MK_ALT;
	if(TVPGetAsyncKeyState(VK_LMENU)) s |= MK_ALT;
	if(TVPGetAsyncKeyState(VK_RMENU)) s |= MK_ALT;
	if(TVPGetAsyncKeyState(VK_SHIFT)) s |= MK_SHIFT;
	if(TVPGetAsyncKeyState(VK_LSHIFT)) s |= MK_SHIFT;
	if(TVPGetAsyncKeyState(VK_RCONTROL)) s |= MK_SHIFT;
	if(TVPGetAsyncKeyState(VK_CONTROL)) s |= MK_CONTROL;
	if(TVPGetAsyncKeyState(VK_LCONTROL)) s |= MK_CONTROL;
	if(TVPGetAsyncKeyState(VK_RCONTROL)) s |= MK_CONTROL;
	return s;
}
static int GetMouseButtonState() {
	int s = 0;
	if(TVPGetAsyncKeyState(VK_LBUTTON)) s |= ssLeft;
	if(TVPGetAsyncKeyState(VK_RBUTTON)) s |= ssRight;
	if(TVPGetAsyncKeyState(VK_MBUTTON)) s |= ssMiddle;
	if(TVPGetAsyncKeyState(VK_XBUTTON1)) s |= ssX1;
	if(TVPGetAsyncKeyState(VK_XBUTTON2)) s |= ssX2;
	return s;
}


class TVPWindowLayer : public TTVPWindowForm {
protected:
	SDL_Window *window;

	TVPWindowLayer *_prevWindow, *_nextWindow;
	SDL_Texture* texture;
	SDL_Renderer* renderer;
	SDL_Surface* surface;
#ifdef KRKRZ_ENABLE_CANVAS
	SDL_GLContext context;
#endif
	tTJSNI_Window *TJSNativeInstance;
	bool hasDrawn = false;
	bool needs_graphic_update = false;
	bool isBeingDeleted = false;
	bool cursor_temporary_hidden = false;
	char* ime_composition;
	size_t ime_composition_cursor;
	size_t ime_composition_len;
	size_t ime_composition_selection;
	SDL_Rect attention_point_rect;
	iTJSDispatch2 * file_drop_array;
	tjs_int file_drop_array_count;
	TVPSDLBitmapCompletion * bitmap_completion;
#ifdef KRKRZ_ENABLE_CANVAS
	tTVPOpenGLScreen * open_gl_screen;
#endif
	int last_mouse_x;
	int last_mouse_y;

public:
	TVPWindowLayer(tTJSNI_Window *w);
	virtual ~TVPWindowLayer();
	virtual void SetPaintBoxSize(tjs_int w, tjs_int h) override;
	virtual bool GetFormEnabled() override;
	virtual void SetDefaultMouseCursor() override;
	virtual void SetMouseCursor(tjs_int handle) override;
	virtual void SetMouseCursorState(tTVPMouseCursorState mcs) override;
	virtual tTVPMouseCursorState GetMouseCursorState() const override;
	void RestoreMouseCursor();
	virtual void HideMouseCursor() override;
	virtual void GetCursorPos(tjs_int &x, tjs_int &y) override;
	virtual void SetCursorPos(tjs_int x, tjs_int y) override;
	virtual void SetAttentionPoint(tjs_int left, tjs_int top, const struct tTVPFont * font) override;
	virtual void BringToFront() override;
	virtual void ShowWindowAsModal() override;
	virtual bool GetVisible() override;
	virtual void SetVisible(bool visible) override;
	virtual void SetFullScreenMode(bool fullscreen) override;
	virtual bool GetFullScreenMode() override;
	virtual void SetBorderStyle(tTVPBorderStyle bs) override;
	virtual tTVPBorderStyle GetBorderStyle() const override;
	virtual tjs_string GetCaption() override;
	virtual void GetCaption(tjs_string & v) const override;
	virtual void SetCaption(const tjs_string & v) override;
	virtual void SetWidth(tjs_int w) override;
	virtual void SetHeight(tjs_int h) override;
	virtual void SetSize(tjs_int w, tjs_int h) override;
	virtual void GetSize(tjs_int &w, tjs_int &h) override;
	virtual tjs_int GetWidth() const override;
	virtual tjs_int GetHeight() const override;
	virtual void SetMinWidth(tjs_int w) override;
	virtual void SetMaxWidth(tjs_int w) override;
	virtual void SetMinHeight(tjs_int h) override;
	virtual void SetMaxHeight(tjs_int h) override;
	virtual void SetMinSize(tjs_int w, tjs_int h) override;
	virtual void SetMaxSize(tjs_int w, tjs_int h) override;
	virtual tjs_int GetMinWidth() override;
	virtual tjs_int GetMaxWidth() override;
	virtual tjs_int GetMinHeight() override;
	virtual tjs_int GetMaxHeight() override;
	virtual tjs_int GetLeft() override;
	virtual void SetLeft(tjs_int l) override;
	virtual tjs_int GetTop() override;
	virtual void SetTop(tjs_int t) override;
	virtual void SetPosition(tjs_int l, tjs_int t) override;
	virtual TVPSDLBitmapCompletion *GetTVPSDLBitmapCompletion() override;
#ifdef KRKRZ_ENABLE_CANVAS
	virtual void SetOpenGLScreen(tTVPOpenGLScreen *s) override;
	virtual void SetSwapInterval(int interval) override;
	virtual void GetDrawableSize(tjs_int &w, tjs_int &h) override;
	virtual void Swap() override;
#endif
	virtual void Show() override;
	virtual void TickBeat() override;
	virtual void InvalidateClose() override;
	virtual bool GetWindowActive() override;
	bool Closing = false, ProgramClosing = false, CanCloseWork = false;
	bool in_mode_ = false; // is modal
	int modal_result_ = 0;
	enum CloseAction {
		caNone,
		caHide,
		caFree,
		caMinimize
	};
	void OnClose(CloseAction& action);
	bool OnCloseQuery();
	virtual void Close() override;
	virtual void OnCloseQueryCalled(bool b) override;
	virtual void SetImeMode(tTVPImeMode mode) override;
	virtual void ResetImeMode() override;
	virtual void UpdateWindow(tTVPUpdateType type) override;
	virtual void InternalKeyDown(tjs_uint16 key, tjs_uint32 shift) override;
	virtual void OnKeyUp(tjs_uint16 vk, int shift) override;
	virtual void OnKeyPress(tjs_uint16 vk, int repeat, bool prevkeystate, bool convertkey) override;
	void window_receive_event(SDL_Event event);
};

TVPWindowLayer::TVPWindowLayer(tTJSNI_Window *w)
{
	ime_composition = nullptr;
	ime_composition_cursor = 0;
	ime_composition_len = 0;
	ime_composition_selection = 0;
	attention_point_rect.x = 0;
	attention_point_rect.y = 0;
	attention_point_rect.w = 0;
	attention_point_rect.h = 0;
	file_drop_array = nullptr;
	file_drop_array_count = 0;
	last_mouse_x = 0;
	last_mouse_y = 0;
	_nextWindow = nullptr;
	_prevWindow = _lastWindowLayer;
	_lastWindowLayer = this;
	if (_prevWindow) {
		_prevWindow->_nextWindow = this;
	}
	if (!_currentWindowLayer) {
		_currentWindowLayer = this;
	}
	if (w) {
		TJSNativeInstance = w;
	}
	else {
		TJSNativeInstance = nullptr;
	}
	
	if (SDL_WasInit(SDL_INIT_VIDEO) == 0)
	{
		if (SDL_Init(SDL_INIT_VIDEO) < 0)
		{
			TVPThrowExceptionMessage(TJS_W("Cannot initialize SDL video subsystem: %1"), ttstr(SDL_GetError()));
		}
		refresh_controllers();
	}

	int new_window_x = SDL_WINDOWPOS_UNDEFINED;
	int new_window_y = SDL_WINDOWPOS_UNDEFINED;
	int new_window_w = 640;
	int new_window_h = 480;
	Uint32 window_flags = 0;

#ifdef SDL_HINT_RENDER_SCALE_QUALITY
	SDL_SetHintWithPriority(SDL_HINT_RENDER_SCALE_QUALITY, "2", SDL_HINT_DEFAULT);
#endif

#ifdef KRKRZ_ENABLE_CANVAS
	if (TVPIsEnableDrawDevice() == false)
	{
		SDL_SetHint(SDL_HINT_OPENGL_ES_DRIVER, "1");
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
		SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
		window_flags |= SDL_WINDOW_OPENGL;
	}
#endif

#ifdef SDL_HINT_TOUCH_MOUSE_EVENTS
	SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "1");
#endif
#ifdef SDL_HINT_MOUSE_TOUCH_EVENTS
	SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
#endif

#ifdef KRKRSDL2_WINDOW_SIZE_IS_LAYER_SIZE
	window_flags |= SDL_WINDOW_RESIZABLE;
	window_flags |= SDL_WINDOW_ALLOW_HIGHDPI;
#ifndef __EMSCRIPTEN__
	window_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
#endif
	new_window_w = 0;
	new_window_h = 0;
#endif

	window = SDL_CreateWindow("krkrsdl2", new_window_x, new_window_y, new_window_w, new_window_h, window_flags);
	if (window == nullptr)
	{
		TVPThrowExceptionMessage(TJS_W("Cannot create SDL window: %1"), ttstr(SDL_GetError()));
	}
#if defined(__EMSCRIPTEN__) && defined(KRKRSDL2_WINDOW_SIZE_IS_LAYER_SIZE)
	EmscriptenFullscreenStrategy strategy;
	strategy.scaleMode = EMSCRIPTEN_FULLSCREEN_CANVAS_SCALE_STDDEF;
	strategy.filteringMode = EMSCRIPTEN_FULLSCREEN_FILTERING_DEFAULT;
	emscripten_enter_soft_fullscreen("canvas", &strategy);           
#endif
#ifdef KRKRZ_ENABLE_CANVAS
	context = nullptr;
	if (TVPIsEnableDrawDevice() == false)
	{
		context = SDL_GL_CreateContext(window);
		if (context == nullptr)
		{
			TVPThrowExceptionMessage(TJS_W("Cannot create SDL context: %1"), ttstr(SDL_GetError()));
		}
		SDL_GL_MakeCurrent(window, context);
	}
#endif
	renderer = nullptr;
	bitmap_completion = nullptr;
#ifdef KRKRZ_ENABLE_CANVAS
	open_gl_screen = nullptr;
#endif
	surface = nullptr;
#ifdef KRKRZ_ENABLE_CANVAS
	if (TVPIsEnableDrawDevice() == true)
#endif
	{
#if !defined(__EMSCRIPTEN__) || (defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__))
		renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
		if (renderer == nullptr)
		{
			TVPAddLog(ttstr("Cannot create SDL renderer: ") + ttstr(SDL_GetError()));
		}
#endif
		bitmap_completion = new TVPSDLBitmapCompletion();
		if (renderer == nullptr)
		{
			surface = SDL_GetWindowSurface(window);
			if (surface == nullptr)
			{
				TVPAddLog(ttstr("Cannot get surface from SDL window: ") + ttstr(SDL_GetError()));
			}
			bitmap_completion->surface = surface;
		}
		if (renderer == nullptr && surface == nullptr)
		{
			TVPThrowExceptionMessage(TJS_W("Cannot get surface or renderer from SDL window"));
		}
		texture = nullptr;
		if (renderer)
		{
			SDL_SetRenderDrawColor( renderer, 0x00, 0x00, 0x00, 0xFF );
		}
	}
}

TVPWindowLayer::~TVPWindowLayer() {
	if (_lastWindowLayer == this) _lastWindowLayer = _prevWindow;
	if (_nextWindow) _nextWindow->_prevWindow = _prevWindow;
	if (_prevWindow) _prevWindow->_nextWindow = _nextWindow;
	if (_currentWindowLayer == this) {
		_currentWindowLayer = _lastWindowLayer;
	}
	if (bitmap_completion)
	{
		delete bitmap_completion;
		bitmap_completion = NULL;
	}
#ifdef KRKRZ_ENABLE_CANVAS
	if (context)
	{
		SDL_GL_DeleteContext(context);
		context = NULL;
	}
#endif
	if (texture && surface)
	{
		SDL_DestroyTexture(texture);
		texture = NULL;
		SDL_FreeSurface(surface);
		surface = NULL;
	}
	if (renderer)
	{
		SDL_DestroyRenderer(renderer);
		renderer = NULL;
	}
	if (window)
	{
		SDL_DestroyWindow(window);
		window = NULL;
	}
}

void TVPWindowLayer::SetPaintBoxSize(tjs_int w, tjs_int h) {
	if (renderer)
	{
		if (texture)
		{
			SDL_DestroyTexture(texture);
			texture = NULL;
		}
		texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STREAMING, w, h);
		if (texture == nullptr)
		{
			TVPThrowExceptionMessage(TJS_W("Cannot create texture texture: %1"), ttstr(SDL_GetError()));
		}
		bitmap_completion->surface = NULL;
		if (surface)
		{
			SDL_FreeSurface(surface);
			surface = NULL;
		}
		surface = SDL_CreateRGBSurface(0, w, h, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0);
		if (surface == nullptr)
		{
			TVPThrowExceptionMessage(TJS_W("Cannot create surface: %1"), ttstr(SDL_GetError()));
		}
		bitmap_completion->surface = surface;
	}
	SDL_Rect cliprect;
	cliprect.x = 0;
	cliprect.y = 0;
	cliprect.w = w;
	cliprect.h = h;
	if (renderer)
	{
		SDL_RenderSetLogicalSize(renderer, w, h);
	}
	if( TJSNativeInstance )
	{
		tTVPRect r;
		r.left = 0;
		r.top = 0;
		r.right = w;
		r.bottom = h;
		TJSNativeInstance->NotifyWindowExposureToLayer(r);
		TJSNativeInstance->GetDrawDevice()->SetClipRectangle(r);
		TJSNativeInstance->GetDrawDevice()->SetDestRectangle(r);
	}
}
bool TVPWindowLayer::GetFormEnabled() {
	if (window)
	{
		return SDL_GetWindowFlags(window) & SDL_WINDOW_SHOWN;
	}
	return false;
}
void TVPWindowLayer::SetDefaultMouseCursor() {
	if (!sdl_system_cursors[0])
	{
		for (int i = 0; i < SDL_NUM_SYSTEM_CURSORS; i += 1)
		{
			sdl_system_cursors[i] = SDL_CreateSystemCursor((SDL_SystemCursor)i);
		}
	}
	SDL_SetCursor(sdl_system_cursors[SDL_SYSTEM_CURSOR_ARROW]);
}
void TVPWindowLayer::SetMouseCursor(tjs_int handle) {
	if (!sdl_system_cursors[0])
	{
		for (int i = 0; i < SDL_NUM_SYSTEM_CURSORS; i += 1)
		{
			sdl_system_cursors[i] = SDL_CreateSystemCursor((SDL_SystemCursor)i);
		}
	}
	switch (handle)
	{
		case -2: // crArrow
			SDL_SetCursor(sdl_system_cursors[SDL_SYSTEM_CURSOR_ARROW]);
			break;
		case -3: // crCross
			SDL_SetCursor(sdl_system_cursors[SDL_SYSTEM_CURSOR_CROSSHAIR]);
			break;
		case -4: // crIBeam
			SDL_SetCursor(sdl_system_cursors[SDL_SYSTEM_CURSOR_IBEAM]);
			break;
		case -5: // crSize
			SDL_SetCursor(sdl_system_cursors[SDL_SYSTEM_CURSOR_SIZEALL]);
			break;
		case -6: // crSizeNESW
			SDL_SetCursor(sdl_system_cursors[SDL_SYSTEM_CURSOR_SIZENESW]);
			break;
		case -7: // crSizeNS
			SDL_SetCursor(sdl_system_cursors[SDL_SYSTEM_CURSOR_SIZENS]);
			break;
		case -8: // crSizeNWSE
			SDL_SetCursor(sdl_system_cursors[SDL_SYSTEM_CURSOR_SIZENWSE]);
			break;
		case -9: // crSizeWE
			SDL_SetCursor(sdl_system_cursors[SDL_SYSTEM_CURSOR_SIZEWE]);
			break;
		case -11: // crHourGlass
			SDL_SetCursor(sdl_system_cursors[SDL_SYSTEM_CURSOR_WAIT]);
			break;
		case -18: // crNo
			SDL_SetCursor(sdl_system_cursors[SDL_SYSTEM_CURSOR_NO]);
			break;
		case -19: // crAppStart
			SDL_SetCursor(sdl_system_cursors[SDL_SYSTEM_CURSOR_WAITARROW]);
			break;
		case -21: // crHandPoint
			SDL_SetCursor(sdl_system_cursors[SDL_SYSTEM_CURSOR_HAND]);
			break;
		case -22: // crSizeAll
			SDL_SetCursor(sdl_system_cursors[SDL_SYSTEM_CURSOR_SIZEALL]);
			break;
		default:
			SDL_SetCursor(sdl_system_cursors[SDL_SYSTEM_CURSOR_ARROW]);
			break;
	}
}
void TVPWindowLayer::SetMouseCursorState(tTVPMouseCursorState mcs) {
	cursor_temporary_hidden = (mcs == mcsTempHidden);
	SDL_ShowCursor((mcs == mcsVisible) ? SDL_ENABLE : SDL_DISABLE);
}
tTVPMouseCursorState TVPWindowLayer::GetMouseCursorState() const {
	if (cursor_temporary_hidden)
	{
		return mcsTempHidden;
	}
	return (SDL_ShowCursor(SDL_QUERY) == SDL_ENABLE) ? mcsVisible : mcsHidden;
}
void TVPWindowLayer::HideMouseCursor() {
	SetMouseCursorState(mcsTempHidden);
}
void TVPWindowLayer::RestoreMouseCursor() {
	if (cursor_temporary_hidden)
	{
		SetMouseCursorState(mcsVisible);
	}
}
void TVPWindowLayer::GetCursorPos(tjs_int &x, tjs_int &y) {
	bool win_has_mouse_focus = window == SDL_GetMouseFocus();
	if (win_has_mouse_focus && renderer)
	{
		tjs_int new_x = 0;
		tjs_int new_y = 0;
		SDL_GetMouseState(&new_x, &new_y);

		float scale_x, scale_y;
		SDL_Rect viewport;
		int window_w, window_h;
		int output_w, output_h;
		SDL_RenderGetScale(renderer, &scale_x, &scale_y);
		SDL_RenderGetViewport(renderer, &viewport);
		SDL_GetWindowSize(window, &window_w, &window_h);
		SDL_GetRendererOutputSize(renderer, &output_w, &output_h);
		float dpi_scale_x = (float)window_w / output_w;
		float dpi_scale_y = (float)window_h / output_h;
		new_x -= (int)(viewport.x * dpi_scale_x);
		new_y -= (int)(viewport.y * dpi_scale_y);
		new_x = (int)(new_x / (scale_x * dpi_scale_x));
		new_y = (int)(new_y / (scale_x * dpi_scale_y));
		x = new_x;
		y = new_y;
		return;
	}
	else if (win_has_mouse_focus)
	{
		SDL_GetMouseState(&x, &y);
		return;
	}
	x = last_mouse_x;
	y = last_mouse_y;
}
void TVPWindowLayer::SetCursorPos(tjs_int x, tjs_int y) {
	RestoreMouseCursor();
	if (renderer && window)
	{
		tjs_int new_x = x;
		tjs_int new_y = y;
		float scale_x, scale_y;
		SDL_Rect viewport;
		int window_w, window_h;
		int output_w, output_h;
		SDL_RenderGetScale(renderer, &scale_x, &scale_y);
		SDL_RenderGetViewport(renderer, &viewport);
		SDL_GetWindowSize(window, &window_w, &window_h);
		SDL_GetRendererOutputSize(renderer, &output_w, &output_h);
		float dpi_scale_x = (float)window_w / output_w;
		float dpi_scale_y = (float)window_h / output_h;
		new_x = (int)(new_x * (scale_x * dpi_scale_x));
		new_y = (int)(new_y * (scale_x * dpi_scale_y));
		new_x += (int)(viewport.x * dpi_scale_x);
		new_y += (int)(viewport.y * dpi_scale_y);
	    SDL_WarpMouseInWindow(window, new_x, new_y);
	}
	else if (window)
	{
		SDL_WarpMouseInWindow(window, x, y);
	}
}
void TVPWindowLayer::SetAttentionPoint(tjs_int left, tjs_int top, const struct tTVPFont * font) {
	attention_point_rect.x = left;
	attention_point_rect.y = top;
	attention_point_rect.w = 0;
	attention_point_rect.h = font->Height;
	SDL_SetTextInputRect(&attention_point_rect);
}
void TVPWindowLayer::BringToFront() {
	if (_currentWindowLayer != this) {
		if (_currentWindowLayer) {
			_currentWindowLayer->TJSNativeInstance->OnReleaseCapture();
		}
		_currentWindowLayer = this;
	}
	if (window)
	{
		SDL_RaiseWindow(window);
	}
}
void TVPWindowLayer::ShowWindowAsModal() {
#if defined(KRKRSDL2_WINDOW_SIZE_IS_LAYER_SIZE)
	TVPThrowExceptionMessage(TJS_W("Showing window as modal is not supported"));
#else
	in_mode_ = true;
	BringToFront();
	modal_result_ = 0;
	while (this == _currentWindowLayer && !modal_result_) {
		process_events();
		if (::Application->IsTarminate()) {
			modal_result_ = mrCancel;
		} else if (modal_result_ != 0) {
			break;
		}
	}
	in_mode_ = false;
#endif
}
bool TVPWindowLayer::GetVisible() {
	if (window)
	{
		return SDL_GetWindowFlags(window) & SDL_WINDOW_SHOWN;
	}
	return false;
}
void TVPWindowLayer::SetVisible(bool visible) {
	if (window)
	{
#ifndef KRKRSDL2_WINDOW_SIZE_IS_LAYER_SIZE
		if (visible)
		{
			SDL_ShowWindow(window);
		}
		else
		{
			SDL_HideWindow(window);
		}
#endif
	}
	if (visible)
	{
		BringToFront();
	}
	else if (!visible && _currentWindowLayer == this)
	{
		_currentWindowLayer = _prevWindow ? _prevWindow : _nextWindow;
		if (_currentWindowLayer)
		{
			_currentWindowLayer->BringToFront();
		}
	}
}
void TVPWindowLayer::SetFullScreenMode(bool fullscreen) {
#ifndef KRKRSDL2_WINDOW_SIZE_IS_LAYER_SIZE
	if (window)
	{
		SDL_SetWindowFullscreen(window, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
	}
	UpdateWindow(utNormal);
#endif
}
bool TVPWindowLayer::GetFullScreenMode() {
#ifndef KRKRSDL2_WINDOW_SIZE_IS_LAYER_SIZE
	if (window)
	{
		return SDL_GetWindowFlags(window) & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP);
	}
#endif
	return false;
}
void TVPWindowLayer::SetBorderStyle(tTVPBorderStyle bs) {
#ifndef KRKRSDL2_WINDOW_SIZE_IS_LAYER_SIZE
	if (window)
	{
		SDL_SetWindowBordered(window, (bs == bsNone) ? SDL_FALSE : SDL_TRUE);
		SDL_SetWindowResizable(window, (bs == bsSizeable || bs == bsSizeToolWin) ? SDL_TRUE : SDL_FALSE);
	}
#endif
}
tTVPBorderStyle TVPWindowLayer::GetBorderStyle() const {
#ifndef KRKRSDL2_WINDOW_SIZE_IS_LAYER_SIZE
	if (window)
	{
		Uint32 flags = SDL_GetWindowFlags(window);
		if (flags & SDL_WINDOW_BORDERLESS)
		{
			return bsNone;
		}
		else if (flags & SDL_WINDOW_RESIZABLE)
		{
			return bsSizeable;
		}
	}
#endif
	return bsSingle;
}
tjs_string TVPWindowLayer::GetCaption() {
	if (window)
	{
		std::string v_utf8 = SDL_GetWindowTitle(window);
		tjs_string v_utf16;
		TVPUtf8ToUtf16( v_utf16, v_utf8 );
		return v_utf16;
	}
	else
	{
		tjs_string empty = TJS_W("");
		return empty;
	}
}
void TVPWindowLayer::GetCaption(tjs_string & v) const {
	v.clear();
	if (window)
	{
		std::string v_utf8 = SDL_GetWindowTitle(window);
		TVPUtf8ToUtf16( v, v_utf8 );
	}
}
void TVPWindowLayer::SetCaption(const tjs_string & v) {
	if (window)
	{
		std::string v_utf8;
		if (TVPUtf16ToUtf8(v_utf8, v))
		{
			SDL_SetWindowTitle(window, v_utf8.c_str());
		}
	}
}
void TVPWindowLayer::SetWidth(tjs_int w) {
#ifndef KRKRSDL2_WINDOW_SIZE_IS_LAYER_SIZE
	if (window)
	{
		int h;
		SDL_GetWindowSize(window, NULL, &h);
		SDL_SetWindowSize(window, w, h);
		if (!renderer && surface)
		{
			bitmap_completion->surface = NULL;
			surface = SDL_GetWindowSurface(window);
			if (surface == nullptr)
			{
				TVPThrowExceptionMessage(TJS_W("Cannot get surface from SDL window: %1"), ttstr(SDL_GetError()));
			}
			bitmap_completion->surface = surface;
		}
	}
#endif
	UpdateWindow(utNormal);
}
void TVPWindowLayer::SetHeight(tjs_int h) {
#ifndef KRKRSDL2_WINDOW_SIZE_IS_LAYER_SIZE
	if (window)
	{
		int w;
		SDL_GetWindowSize(window, &w, NULL);
		SDL_SetWindowSize(window, w, h);
		if (!renderer && surface)
		{
			bitmap_completion->surface = NULL;
			surface = SDL_GetWindowSurface(window);
			if (surface == nullptr)
			{
				TVPThrowExceptionMessage(TJS_W("Cannot get surface from SDL window: %1"), ttstr(SDL_GetError()));
			}
			bitmap_completion->surface = surface;
		}
	}
#endif
	UpdateWindow(utNormal);
}
void TVPWindowLayer::SetSize(tjs_int w, tjs_int h) {
#ifndef KRKRSDL2_WINDOW_SIZE_IS_LAYER_SIZE
	if (window)
	{
		SDL_SetWindowSize(window, w, h);
		if (!renderer && surface)
		{
			bitmap_completion->surface = NULL;
			surface = SDL_GetWindowSurface(window);
			if (surface == nullptr)
			{
				TVPThrowExceptionMessage(TJS_W("Cannot get surface from SDL window: %1"), ttstr(SDL_GetError()));
			}
			bitmap_completion->surface = surface;
		}
	}
#endif
	UpdateWindow(utNormal);
}
void TVPWindowLayer::GetSize(tjs_int &w, tjs_int &h) {
#ifndef KRKRSDL2_WINDOW_SIZE_IS_LAYER_SIZE
	if (window)
	{
		SDL_GetWindowSize(window, &w, &h);
		return;
	}
#endif
	if (renderer)
	{
		SDL_RenderGetLogicalSize(renderer, &w, &h);
		return;
	}
	w = 0;
	h = 0;
}
tjs_int TVPWindowLayer::GetWidth() const {
#ifndef KRKRSDL2_WINDOW_SIZE_IS_LAYER_SIZE
	if (window)
	{
		int w;
		SDL_GetWindowSize(window, &w, NULL);
		return w;
	}
#endif
	if (renderer)
	{
		int w, h;
		SDL_RenderGetLogicalSize(renderer, &w, &h);
		return w;
	}
	return 0;
}
tjs_int TVPWindowLayer::GetHeight() const {
#ifndef KRKRSDL2_WINDOW_SIZE_IS_LAYER_SIZE
	if (window)
	{
		int h;
		SDL_GetWindowSize(window, NULL, &h);
		return h;
	}
#endif
	if (renderer)
	{
		int w, h;
		SDL_RenderGetLogicalSize(renderer, &w, &h);
		return h;
	}
	return 0;
}
void TVPWindowLayer::SetMinWidth(tjs_int w) {
	SetMinSize(w, GetMinHeight());
}
void TVPWindowLayer::SetMaxWidth(tjs_int w) {
	SetMaxSize(w, GetMaxHeight());
}
void TVPWindowLayer::SetMinHeight(tjs_int h) {
	SetMinSize(GetMinWidth(), h);
}
void TVPWindowLayer::SetMaxHeight(tjs_int h) {
	SetMaxSize(GetMaxWidth(), h);
}
void TVPWindowLayer::SetMinSize(tjs_int w, tjs_int h) {
#ifndef KRKRSDL2_WINDOW_SIZE_IS_LAYER_SIZE
	if (window)
	{
		SDL_SetWindowMinimumSize(window, w, h);
	}
#endif
}
void TVPWindowLayer::SetMaxSize(tjs_int w, tjs_int h) {
#ifndef KRKRSDL2_WINDOW_SIZE_IS_LAYER_SIZE
	if (window)
	{
		SDL_SetWindowMaximumSize(window, w, h);
	}
#endif
}
tjs_int TVPWindowLayer::GetMinWidth() {
#ifndef KRKRSDL2_WINDOW_SIZE_IS_LAYER_SIZE
	if (window)
	{
		int w;
		SDL_GetWindowMinimumSize(window, &w, nullptr);
		return w;
	}
#endif
	if (renderer)
	{
		int w, h;
		SDL_RenderGetLogicalSize(renderer, &w, &h);
		return w;
	}
	return 0;
}
tjs_int TVPWindowLayer::GetMaxWidth() {
#ifndef KRKRSDL2_WINDOW_SIZE_IS_LAYER_SIZE
	if (window)
	{
		int w;
		SDL_GetWindowMaximumSize(window, &w, nullptr);
		return w;
	}
#endif
	if (renderer)
	{
		int w, h;
		SDL_RenderGetLogicalSize(renderer, &w, &h);
		return w;
	}
	return 0;
}
tjs_int TVPWindowLayer::GetMinHeight() {
#ifndef KRKRSDL2_WINDOW_SIZE_IS_LAYER_SIZE
	if (window)
	{
		int h;
		SDL_GetWindowMinimumSize(window, &h, nullptr);
		return h;
	}
#endif
	if (renderer)
	{
		int w, h;
		SDL_RenderGetLogicalSize(renderer, &w, &h);
		return h;
	}
	return 0;
}
tjs_int TVPWindowLayer::GetMaxHeight() {
#ifndef KRKRSDL2_WINDOW_SIZE_IS_LAYER_SIZE
	if (window)
	{
		int h;
		SDL_GetWindowMaximumSize(window, &h, nullptr);
		return h;
	}
#endif
	if (renderer)
	{
		int w, h;
		SDL_RenderGetLogicalSize(renderer, &w, &h);
		return h;
	}
	return 0;
}
tjs_int TVPWindowLayer::GetLeft() {
#ifndef KRKRSDL2_WINDOW_SIZE_IS_LAYER_SIZE
	if (window)
	{
		int x;
		SDL_GetWindowPosition(window, &x, nullptr);
		return x;
	}
#endif
	return 0;
}
void TVPWindowLayer::SetLeft(tjs_int l) {
	SetPosition(l, GetTop());
}
tjs_int TVPWindowLayer::GetTop() {
#ifndef KRKRSDL2_WINDOW_SIZE_IS_LAYER_SIZE
	if (window)
	{
		int y;
		SDL_GetWindowPosition(window, nullptr, &y);
		return y;
	}
#endif
	return 0;
}
void TVPWindowLayer::SetTop(tjs_int t) {
	SetPosition(GetLeft(), t);
}
void TVPWindowLayer::SetPosition(tjs_int l, tjs_int t) {
#ifndef KRKRSDL2_WINDOW_SIZE_IS_LAYER_SIZE
	if (window)
	{
		SDL_SetWindowPosition(window, l, t);
	}
#endif
}
TVPSDLBitmapCompletion *TVPWindowLayer::GetTVPSDLBitmapCompletion() {
	needs_graphic_update = true;
	return bitmap_completion;
}
#ifdef KRKRZ_ENABLE_CANVAS
void TVPWindowLayer::SetOpenGLScreen(tTVPOpenGLScreen *s) {
	open_gl_screen = s;
}
void TVPWindowLayer::SetSwapInterval(int interval) {
	if (context)
	{
		SDL_GL_SetSwapInterval(interval);
	}
}
void TVPWindowLayer::GetDrawableSize(tjs_int &w, tjs_int &h) {
	if (context)
	{
		SDL_GL_GetDrawableSize(window, &w, &h);
	}
	else
	{
		w = 0;
		h = 0;
	}
}
void TVPWindowLayer::Swap() {
	if (context)
	{
		SDL_GL_SwapWindow(window);
	}
}
#endif
void TVPWindowLayer::Show() {
}
void TVPWindowLayer::TickBeat() {
	if (needs_graphic_update)
	{
		if (renderer && bitmap_completion)
		{
			SDL_Rect rect;
			rect.x = bitmap_completion->update_rect.left;
			rect.y = bitmap_completion->update_rect.top;
			rect.w = bitmap_completion->update_rect.get_width();
			rect.h = bitmap_completion->update_rect.get_height();
			SDL_Rect logical_rect;
			SDL_RenderGetLogicalSize(renderer, &(logical_rect.w), &(logical_rect.h));
			if (logical_rect.w == rect.w && logical_rect.h == rect.h)
			{
				// Clear extra artifacts
				SDL_RenderSetLogicalSize(renderer, 0, 0);
				SDL_RenderFillRect(renderer, NULL);
				SDL_RenderSetLogicalSize(renderer, logical_rect.w, logical_rect.h);
			}
			if (texture && surface)
			{
				if ((rect.w + rect.x) > surface->w)
				{
					rect.w = surface->w;
				}
				if ((rect.h + rect.y) > surface->h)
				{
					rect.h = surface->h;
				}
				SDL_UpdateTexture(texture, &rect, surface->pixels, surface->pitch);
				SDL_RenderCopy(renderer, texture, &rect, &rect);
			}
			SDL_RenderPresent(renderer);
			if (logical_rect.w == rect.w && logical_rect.h == rect.h)
			{
				// Clear extra artifacts (for the back buffer)
				SDL_RenderSetLogicalSize(renderer, 0, 0);
				SDL_RenderFillRect(renderer, NULL);
				SDL_RenderSetLogicalSize(renderer, logical_rect.w, logical_rect.h);
			}
			if (texture)
			{
				SDL_RenderCopy(renderer, texture, &rect, &rect);
			}
			hasDrawn = true;
		}
		else if (window && surface)
		{
			SDL_UpdateWindowSurface(window);
			hasDrawn = true;
		}
		needs_graphic_update = false;
	}
#ifdef KRKRZ_ENABLE_CANVAS
	else if (context && TJSNativeInstance)
	{
		TJSNativeInstance->StartDrawing();
		hasDrawn = true;
	}
#endif
}
void TVPWindowLayer::InvalidateClose() {
	TJSNativeInstance = NULL;
	SetVisible(false);
	delete this;
}
bool TVPWindowLayer::GetWindowActive() {
	return _currentWindowLayer == this;
}
void TVPWindowLayer::OnClose(CloseAction& action) {
	if (modal_result_ == 0)
		action = caNone;
	else
		action = caHide;

	if (ProgramClosing) {
		if (TJSNativeInstance) {
			if (TJSNativeInstance->IsMainWindow()) {
				// this is the main window
			} else 			{
				// not the main window
				action = caFree;
			}
			iTJSDispatch2 * obj = TJSNativeInstance->GetOwnerNoAddRef();
			TJSNativeInstance->NotifyWindowClose();
			obj->Invalidate(0, NULL, NULL, obj);
			TJSNativeInstance = NULL;
			SetVisible(false);
		}
	}
}
bool TVPWindowLayer::OnCloseQuery() {
	// closing actions are 3 patterns;
	// 1. closing action by the user
	// 2. "close" method
	// 3. object invalidation

	if (TVPGetBreathing()) {
		return false;
	}

	// the default event handler will invalidate this object when an onCloseQuery
	// event reaches the handler.
	if (TJSNativeInstance && (modal_result_ == 0 ||
		modal_result_ == mrCancel/* mrCancel=when close button is pushed in modal window */)) {
		iTJSDispatch2 * obj = TJSNativeInstance->GetOwnerNoAddRef();
		if (obj) {
			tTJSVariant arg[1] = { true };
			static ttstr eventname(TJS_W("onCloseQuery"));

			if (!ProgramClosing) {
				// close action does not happen immediately
				if (TJSNativeInstance) {
					TVPPostInputEvent(new tTVPOnCloseInputEvent(TJSNativeInstance));
				}

				Closing = true; // waiting closing...
			//	TVPSystemControl->NotifyCloseClicked();
				return false;
			} else {
				CanCloseWork = true;
				TVPPostEvent(obj, obj, eventname, 0, TVP_EPT_IMMEDIATE, 1, arg);
				process_events(); // for post event
				// this event happens immediately
				// and does not return until done
				return CanCloseWork; // CanCloseWork is set by the event handler
			}
		} else {
			return true;
		}
	} else {
		return true;
	}
}
void TVPWindowLayer::Close() {
	// closing action by "close" method
	if (Closing) return; // already waiting closing...

	ProgramClosing = true;
	try {
		//tTVPWindow::Close();
		if (in_mode_) {
			modal_result_ = mrCancel;
		} 
		else if (OnCloseQuery()) {
			CloseAction action = caFree;
			OnClose(action);
			switch (action) {
			case caNone:
				break;
			case caHide:
				SetVisible(false);
				break;
			case caMinimize:
				if (window)
				{
					SDL_MinimizeWindow(window);
				}
				break;
			case caFree:
			default:
				isBeingDeleted = true;
				//::PostMessage(GetHandle(), TVP_EV_WINDOW_RELEASE, 0, 0);
				break;
			}
		}
	}
	catch (...) {
		ProgramClosing = false;
		throw;
	}
	ProgramClosing = false;
}
void TVPWindowLayer::OnCloseQueryCalled(bool b) {
	// closing is allowed by onCloseQuery event handler
	if (!ProgramClosing) {
		// closing action by the user
		if (b) {
			if (in_mode_)
				modal_result_ = 1; // when modal
			else
				SetVisible(false);  // just hide

			Closing = false;
			if (TJSNativeInstance) {
				if (TJSNativeInstance->IsMainWindow()) {
					// this is the main window
					iTJSDispatch2 * obj = TJSNativeInstance->GetOwnerNoAddRef();
					obj->Invalidate(0, NULL, NULL, obj);
				}
			} else {
				delete this;
			}
		} else {
			Closing = false;
		}
	} else {
		// closing action by the program
		CanCloseWork = b;
	}
}
void TVPWindowLayer::SetImeMode(tTVPImeMode mode) {
	if (!window || mode == ::imDisable || mode == ::imClose)
	{
		ResetImeMode();
	}
	else
	{
		if (!SDL_IsTextInputActive())
		{
			SDL_SetTextInputRect(&attention_point_rect);
			SDL_StartTextInput();
		}
	}
}
void TVPWindowLayer::ResetImeMode() {
	ime_composition = nullptr;
	ime_composition_len = 0;
	ime_composition_cursor = 0;
	ime_composition_selection = 0;
	attention_point_rect.x = 0;
	attention_point_rect.y = 0;
	attention_point_rect.w = 0;
	attention_point_rect.h = 0;
	if (window && SDL_IsTextInputActive())
	{
		SDL_SetTextInputRect(&attention_point_rect);
		SDL_StopTextInput();
	}
}
void TVPWindowLayer::UpdateWindow(tTVPUpdateType type) {
	if (TJSNativeInstance) {
		tTVPRect r;
		r.clear();
		if (renderer)
		{
			SDL_RenderGetLogicalSize(renderer, &(r.right), &(r.bottom));
			SDL_RenderSetLogicalSize(renderer, r.right, r.bottom);
		}
		else if (window)
		{
			SDL_GetWindowSize(window, &(r.right), &(r.bottom));
		}
		TJSNativeInstance->NotifyWindowExposureToLayer(r);
		TVPDeliverWindowUpdateEvents();
	}
}
void TVPWindowLayer::InternalKeyDown(tjs_uint16 key, tjs_uint32 shift) {
	TVPPostInputEvent(new tTVPOnKeyDownInputEvent(TJSNativeInstance, key, shift));
}
void TVPWindowLayer::OnKeyUp(tjs_uint16 vk, int shift) {
	TVPPostInputEvent(new tTVPOnKeyUpInputEvent(TJSNativeInstance, vk, shift));
}
void TVPWindowLayer::OnKeyPress(tjs_uint16 vk, int repeat, bool prevkeystate, bool convertkey) {
	TVPPostInputEvent(new tTVPOnKeyPressInputEvent(TJSNativeInstance, vk));
}
void TVPWindowLayer::window_receive_event(SDL_Event event) {
	if (isBeingDeleted) {
		delete this;
		return;
	}
	if (window && _prevWindow) {
		uint32_t windowID = SDL_GetWindowID(window);
		bool tryParentWindow = false;
		switch (event.type) {
			case SDL_DROPFILE:
			case SDL_DROPTEXT:
			case SDL_DROPBEGIN:
			case SDL_DROPCOMPLETE:
				tryParentWindow = event.drop.windowID != windowID;
				break;
			case SDL_KEYDOWN:
			case SDL_KEYUP:
				tryParentWindow = event.key.windowID != windowID;
				break;
			case SDL_MOUSEMOTION:
				tryParentWindow = event.motion.windowID != windowID;
				break;
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
				tryParentWindow = event.button.windowID != windowID;
				break;
			case SDL_MOUSEWHEEL:
				tryParentWindow = event.wheel.windowID != windowID;
				break;
			case SDL_TEXTEDITING:
				tryParentWindow = event.edit.windowID != windowID;
				break;
			case SDL_TEXTINPUT:
				tryParentWindow = event.text.windowID != windowID;
				break;
			case SDL_WINDOWEVENT:
				tryParentWindow = event.window.windowID != windowID;
				break;
			default:
				tryParentWindow = false;
				break;
		}
		if (tryParentWindow) {
			if (!in_mode_) {
				_prevWindow->window_receive_event(event);
			}
			return;
		}
	}
	if (window && hasDrawn) {
		tjs_uint32 s = TVP_TShiftState_To_uint32(GetShiftState());
		s |= GetMouseButtonState();
		if (TJSNativeInstance->CanDeliverEvents()) {
			switch (event.type) { 
				case SDL_MOUSEMOTION: {
					RestoreMouseCursor();
					last_mouse_x = event.motion.x;
					last_mouse_y = event.motion.y;
					TVPPostInputEvent(new tTVPOnMouseMoveInputEvent(TJSNativeInstance, last_mouse_x, last_mouse_y, s));
					return;
				}
				case SDL_MOUSEBUTTONDOWN:
				case SDL_MOUSEBUTTONUP: {
					if (SDL_IsTextInputActive() && ime_composition != nullptr)
					{
						return;
					}
					tTVPMouseButton btn;
					bool hasbtn = true;
					switch(event.button.button) {
						case SDL_BUTTON_RIGHT:
							btn = tTVPMouseButton::mbRight;
							break;
						case SDL_BUTTON_MIDDLE:
							btn = tTVPMouseButton::mbMiddle;
							break;
						case SDL_BUTTON_LEFT:
							btn = tTVPMouseButton::mbLeft;
							break;
						case SDL_BUTTON_X1:
							btn = tTVPMouseButton::mbX1;
							break;
						case SDL_BUTTON_X2:
							btn = tTVPMouseButton::mbX2;
							break;
						default:
							hasbtn = false;
							break;
					}
					if (hasbtn) {
						last_mouse_x = event.button.x;
						last_mouse_y = event.button.y;
						TVPPostInputEvent(new tTVPOnMouseMoveInputEvent(TJSNativeInstance, last_mouse_x, last_mouse_y, s));
						switch (event.type) {
							case SDL_MOUSEBUTTONDOWN:
								TVPPostInputEvent(new tTVPOnMouseDownInputEvent(TJSNativeInstance, last_mouse_x, last_mouse_y, btn, s));
								break;
							case SDL_MOUSEBUTTONUP:
								TVPPostInputEvent(new tTVPOnClickInputEvent(TJSNativeInstance, last_mouse_x, last_mouse_y));
								TVPPostInputEvent(new tTVPOnMouseUpInputEvent(TJSNativeInstance, last_mouse_x, last_mouse_y, btn, s));
								break;
						}
					}
					return;
				}
				case SDL_CONTROLLERBUTTONDOWN:
				case SDL_CONTROLLERBUTTONUP: {
					switch (event.cbutton.state) {
						case SDL_PRESSED:
							TVPPostInputEvent(new tTVPOnKeyDownInputEvent(TJSNativeInstance, sdl_gamecontrollerbutton_to_vk_key(event.cbutton.button), s));
							break;
						case SDL_RELEASED:
							if (!SDL_IsTextInputActive())
							{
								TVPPostInputEvent(new tTVPOnKeyPressInputEvent(TJSNativeInstance, sdl_gamecontrollerbutton_to_vk_key(event.cbutton.button)));
							}
							TVPPostInputEvent(new tTVPOnKeyUpInputEvent(TJSNativeInstance, sdl_gamecontrollerbutton_to_vk_key(event.cbutton.button), s));
							break;
					}
					return;
				}
				case SDL_KEYDOWN: {
					if (SDL_IsTextInputActive())
					{
						if (ime_composition != nullptr)
						{
							return;
						}
					}
					if (event.key.repeat) s |= TVP_SS_REPEAT;
					tjs_uint unified_vk_key = 0;
					switch (event.key.keysym.sym)
					{
						case SDLK_LSHIFT:
						case SDLK_RSHIFT:
							unified_vk_key = VK_SHIFT;
							break;
						case SDLK_LCTRL:
						case SDLK_RCTRL:
							unified_vk_key = VK_CONTROL;
							break;
						case SDLK_LALT:
						case SDLK_RALT:
							unified_vk_key = VK_MENU;
							break;
					}
					TVPPostInputEvent(new tTVPOnKeyDownInputEvent(TJSNativeInstance, sdl_key_to_vk_key(event.key.keysym.sym), s));
					if (unified_vk_key)
					{
						TVPPostInputEvent(new tTVPOnKeyDownInputEvent(TJSNativeInstance, unified_vk_key, s));
					}
					SDL_SetTextInputRect(&attention_point_rect);
					return;
				}
				case SDL_KEYUP: {
					if (SDL_IsTextInputActive())
					{
						if (ime_composition != nullptr)
						{
							return;
						}
					}
					tjs_uint unified_vk_key = 0;
					switch (event.key.keysym.sym)
					{
						case SDLK_LSHIFT:
						case SDLK_RSHIFT:
							unified_vk_key = VK_SHIFT;
							break;
						case SDLK_LCTRL:
						case SDLK_RCTRL:
							unified_vk_key = VK_CONTROL;
							break;
						case SDLK_LALT:
						case SDLK_RALT:
							unified_vk_key = VK_MENU;
							break;
					}
					if (!SDL_IsTextInputActive())
					{
						TVPPostInputEvent(new tTVPOnKeyPressInputEvent(TJSNativeInstance, sdl_key_to_vk_key(event.key.keysym.sym)));
						if (unified_vk_key)
						{
							TVPPostInputEvent(new tTVPOnKeyPressInputEvent(TJSNativeInstance, unified_vk_key));
						}
					}
					TVPPostInputEvent(new tTVPOnKeyUpInputEvent(TJSNativeInstance, sdl_key_to_vk_key(event.key.keysym.sym), s));
					if (unified_vk_key)
					{
						TVPPostInputEvent(new tTVPOnKeyUpInputEvent(TJSNativeInstance, unified_vk_key, s));
					}
					SDL_SetTextInputRect(&attention_point_rect);
					return;
				}
				case SDL_TEXTINPUT:
				case SDL_TEXTEDITING: {
					if (!SDL_IsTextInputActive())
					{
						return;
					}
					// TODO: figure out vertical edit
					for (size_t i = 0; i < ime_composition_selection; i += 1)
					{
						TVPPostInputEvent(new tTVPOnKeyDownInputEvent(TJSNativeInstance, VK_LEFT, TVP_SS_SHIFT));
						TVPPostInputEvent(new tTVPOnKeyUpInputEvent(TJSNativeInstance, VK_LEFT, TVP_SS_SHIFT));
#if 0
						TVPPostInputEvent(new tTVPOnKeyDownInputEvent(TJSNativeInstance, VK_DOWN, TVP_SS_SHIFT));
						TVPPostInputEvent(new tTVPOnKeyUpInputEvent(TJSNativeInstance, VK_DOWN, TVP_SS_SHIFT));
#endif
					}
					for (size_t i = 0; i < ime_composition_len - ime_composition_cursor; i += 1)
					{
						TVPPostInputEvent(new tTVPOnKeyDownInputEvent(TJSNativeInstance, VK_RIGHT, 0));
						TVPPostInputEvent(new tTVPOnKeyUpInputEvent(TJSNativeInstance, VK_RIGHT, 0));
#if 0
						TVPPostInputEvent(new tTVPOnKeyDownInputEvent(TJSNativeInstance, VK_DOWN, 0));
						TVPPostInputEvent(new tTVPOnKeyUpInputEvent(TJSNativeInstance, VK_DOWN, 0));
#endif
					}
					for (size_t i = 0; i < ime_composition_len; i += 1)
					{
						TVPPostInputEvent(new tTVPOnKeyDownInputEvent(TJSNativeInstance, VK_BACK, 0));
						TVPPostInputEvent(new tTVPOnKeyUpInputEvent(TJSNativeInstance, VK_BACK, 0));
					}
					switch (event.type) {
						case SDL_TEXTINPUT:
							ime_composition = event.text.text;
							ime_composition_cursor = 0;
							ime_composition_selection = 0;
							break;
						case SDL_TEXTEDITING:
							ime_composition = event.edit.text;
							ime_composition_cursor = event.edit.start;
							ime_composition_selection = event.edit.length;
							break;
					}
					size_t buffer_len = TVPUtf8ToWideCharString((const char*)(ime_composition), NULL);
					if (buffer_len == (size_t)-1)
					{
						return;
					}
					if (buffer_len != 0)
					{
						ime_composition_len = buffer_len;
						tjs_char *buffer = new tjs_char[buffer_len + 1];
						TVPUtf8ToWideCharString((const char*)(ime_composition), buffer);
						for (size_t i = 0; i < buffer_len; i += 1)
						{
							TVPPostInputEvent(new tTVPOnKeyPressInputEvent(TJSNativeInstance, buffer[i]));
						}
						delete[] buffer;
					}
					else
					{
						ime_composition = nullptr;
						ime_composition_len = 0;
						ime_composition_cursor = 0;
						ime_composition_selection = 0;
					}
					if (event.type == SDL_TEXTEDITING)
					{
						for (size_t i = 0; i < ime_composition_len - ime_composition_cursor; i += 1)
						{
							TVPPostInputEvent(new tTVPOnKeyDownInputEvent(TJSNativeInstance, VK_LEFT, 0));
							TVPPostInputEvent(new tTVPOnKeyUpInputEvent(TJSNativeInstance, VK_LEFT, 0));
#if 0
							TVPPostInputEvent(new tTVPOnKeyDownInputEvent(TJSNativeInstance, VK_UP, 0));
							TVPPostInputEvent(new tTVPOnKeyUpInputEvent(TJSNativeInstance, VK_UP, 0));
#endif
						}
						for (size_t i = 0; i < ime_composition_selection; i += 1)
						{
							TVPPostInputEvent(new tTVPOnKeyDownInputEvent(TJSNativeInstance, VK_RIGHT, TVP_SS_SHIFT));
							TVPPostInputEvent(new tTVPOnKeyUpInputEvent(TJSNativeInstance, VK_RIGHT, TVP_SS_SHIFT));
#if 0
							TVPPostInputEvent(new tTVPOnKeyDownInputEvent(TJSNativeInstance, VK_DOWN, TVP_SS_SHIFT));
							TVPPostInputEvent(new tTVPOnKeyUpInputEvent(TJSNativeInstance, VK_DOWN, TVP_SS_SHIFT));
#endif
						}
					}
					if (event.type == SDL_TEXTINPUT)
					{
						ime_composition = nullptr;
						ime_composition_len = 0;
					}
					return;
				}
				case SDL_MOUSEWHEEL: {
					TVPPostInputEvent(new tTVPOnMouseWheelInputEvent(TJSNativeInstance, event.wheel.x, event.wheel.y, last_mouse_x, last_mouse_y));
					return;
				}
				case SDL_DROPBEGIN: {
					if (!file_drop_array)
					{
						file_drop_array = TJSCreateArrayObject();
					}
					return;
				}
				case SDL_DROPCOMPLETE: {
					if (file_drop_array)
					{
						tTJSVariant arg(file_drop_array, file_drop_array);
						TVPPostInputEvent(new tTVPOnFileDropInputEvent(TJSNativeInstance, arg));
						file_drop_array->Release();
						file_drop_array = nullptr;
						file_drop_array_count = 0;
					}
					return;
				}
				case SDL_DROPFILE:
				case SDL_DROPTEXT: {
					if (file_drop_array && event.drop.file)
					{
						std::string f_utf8 = event.drop.file;
						tjs_string f_utf16;
						TVPUtf8ToUtf16( f_utf16, f_utf8 );
						SDL_free(event.drop.file);
						if (TVPIsExistentStorageNoSearch(f_utf16))
						{
							tTJSVariant val = TVPNormalizeStorageName(ttstr(f_utf16));
							file_drop_array->PropSetByNum(TJS_MEMBERENSURE|TJS_IGNOREPROP, file_drop_array_count, &val, file_drop_array);
							file_drop_array_count += 1;
						}
					}
					return;
				}
				case SDL_CONTROLLERDEVICEADDED:
				case SDL_CONTROLLERDEVICEREMOVED:
				case SDL_CONTROLLERDEVICEREMAPPED:
				{
					refresh_controllers();
					return;
				}
				case SDL_WINDOWEVENT: {
					switch (event.window.event)
					{
						case SDL_WINDOWEVENT_EXPOSED: {
							UpdateWindow(utNormal);
							return;
						}
						case SDL_WINDOWEVENT_MINIMIZED:
						case SDL_WINDOWEVENT_MAXIMIZED:
						case SDL_WINDOWEVENT_RESTORED:
						case SDL_WINDOWEVENT_RESIZED:
						case SDL_WINDOWEVENT_SIZE_CHANGED: {
							UpdateWindow(utNormal);
							TVPPostInputEvent(new tTVPOnResizeInputEvent(TJSNativeInstance), TVP_EPT_REMOVE_POST);
							return;
						}
						case SDL_WINDOWEVENT_ENTER: {
							TVPPostInputEvent(new tTVPOnMouseEnterInputEvent(TJSNativeInstance));
							return;
						}
						case SDL_WINDOWEVENT_LEAVE: {
							TVPPostInputEvent(new tTVPOnMouseOutOfWindowInputEvent(TJSNativeInstance));
							TVPPostInputEvent(new tTVPOnMouseLeaveInputEvent(TJSNativeInstance));
							return;
						}
						case SDL_WINDOWEVENT_FOCUS_GAINED:
						case SDL_WINDOWEVENT_FOCUS_LOST: {
							TVPPostInputEvent(new tTVPOnWindowActivateEvent(TJSNativeInstance, event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED), TVP_EPT_REMOVE_POST);
							return;
						}
						case SDL_WINDOWEVENT_CLOSE: {
							TVPPostInputEvent(new tTVPOnCloseInputEvent(TJSNativeInstance));
							return;
						}
						default: {
							return;
						}
					}
				}
				case SDL_QUIT: {
					TVPPostInputEvent(new tTVPOnCloseInputEvent(TJSNativeInstance));
					return;
				}
				default: {
					return;
				}
			}
		}
	}
}

void sdl_process_events()
{
	if (SDL_WasInit(SDL_INIT_EVENTS) != 0)
	{
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			if (event.type == native_event_queue_custom_event_type)
			{
				((NativeEventQueueImplement *)(event.user.data1))->Dispatch(*((NativeEvent*)event.user.data2));
			}
			else
			{
				if (_currentWindowLayer)
				{
					_currentWindowLayer->window_receive_event(event);
				}
				else
				{
					if (event.type == SDL_QUIT)
					{
						Application->Terminate();
					}
				}
			}
		}
	}
}

#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
static void process_events()
#else
static bool process_events()
#endif
{
	try
	{
		try
		{
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
			tTJSNI_WaveSoundBuffer::Trigger();
#endif
			::Application->Run();
			if (::Application->IsTarminate())
			{
				TVPSystemUninit();
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
				emscripten_cancel_main_loop();
#else
				return false;
#endif
			}
		}
		TJS_CONVERT_TO_TJS_EXCEPTION
	}
	TVP_CATCH_AND_SHOW_SCRIPT_EXCEPTION(TJS_W("SDL event processing"));

#if !defined(__EMSCRIPTEN__) || (defined(__EMSCRIPTEN__) && defined(__EMSCRIPTEN_PTHREADS__))
	return true;
#endif
}

#if defined(USE_SDL_MAIN) && defined(__IPHONEOS__)
static int SDL_actual_main(int argc, char **argv)
#elif defined(USE_SDL_MAIN)
int SDL_main(int argc, char **argv)
#else
int main(int argc, char **argv)
#endif
{
	_argc = argc;
	_wargv = new tjs_char*[argc];

	for (int i = 0; i < argc; i += 1)
	{
		const char* narg;
#ifndef __EMSCRIPTEN__
		if (!i)
		{
			narg = realpath(argv[i], NULL);
		}
		else
#endif
		{
			narg = argv[i];
		}
		if (!narg)
		{
			tjs_char* warg_copy = new tjs_char[1];
			warg_copy[0] = '\0';
			_wargv[i] = warg_copy;
			continue;
		}
		std::string v_utf8 = narg;
		tjs_string v_utf16;
		TVPUtf8ToUtf16( v_utf16, v_utf8 );
#ifndef __EMSCRIPTEN__
		if (!i)
		{
			free((void*)narg);
		}
#endif
		tjs_char* warg_copy = new tjs_char[v_utf16.length() + 1];
		memcpy(warg_copy, v_utf16.c_str(), sizeof(tjs_char) * (v_utf16.length()));
		warg_copy[v_utf16.length()] = '\0';
		_wargv[i] = warg_copy;
	}

	setenv("DBUS_FATAL_WARNINGS", "0", 1);

	TVPLoadMessage();

#ifdef TVP_LOG_TO_COMMANDLINE_CONSOLE
	SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_VERBOSE);
#endif

	::Application = new tTVPApplication();
	if (::Application->StartApplication( _argc, _wargv ))
	{
		return 0;
	}

#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
	emscripten_set_main_loop(process_events, 0, 0);
#else
	while (process_events());
#endif
	return 0;
}

#if defined(USE_SDL_MAIN) && defined(__IPHONEOS__)
int SDL_main(int argc, char **argv)
{
	return SDL_UIKitRunApp(argc, argv, SDL_actual_main);
}
#endif

bool TVPGetKeyMouseAsyncState(tjs_uint keycode, bool getcurrent)
{
	if(keycode >= VK_LBUTTON && keycode <= VK_XBUTTON2 && keycode != VK_CANCEL)
	{
		Uint32 state = SDL_GetMouseState(NULL, NULL);
		switch (keycode) {
			case VK_LBUTTON:
				return !!(state & SDL_BUTTON(SDL_BUTTON_LEFT));
			case VK_RBUTTON:
				return !!(state & SDL_BUTTON(SDL_BUTTON_RIGHT));
			case VK_MBUTTON:
				return !!(state & SDL_BUTTON(SDL_BUTTON_MIDDLE));
			case VK_XBUTTON1:
				return !!(state & SDL_BUTTON(SDL_BUTTON_X1));
			case VK_XBUTTON2:
				return !!(state & SDL_BUTTON(SDL_BUTTON_X2));
			default:
				return false;
		}
	}
	if(keycode >= VK_SHIFT && keycode <= VK_MENU)
	{
		Uint32 state = SDL_GetModState();
		switch (keycode) {
			case VK_SHIFT:
				return !!(state & KMOD_SHIFT);
			case VK_MENU:
				return !!(state & KMOD_ALT);
			case VK_CONTROL:
				return !!(state & KMOD_CTRL);
			default:
				return false;
		}
	}
	const Uint8 *state = SDL_GetKeyboardState(NULL);
	return !!(state[SDL_GetScancodeFromKey(vk_key_to_sdl_key(keycode))]);
}

bool TVPGetJoyPadAsyncState(tjs_uint keycode, bool getcurrent)
{
	bool is_pressed = false;
	for (int i = 0; i < sdl_controller_num; i += 1)
	{
		if (sdl_controllers[i])
		{
			is_pressed |= !!SDL_GameControllerGetButton(sdl_controllers[i], (SDL_GameControllerButton)vk_key_to_sdl_gamecontrollerbutton(keycode));
		}
	}
	return is_pressed;
}

TTVPWindowForm *TVPCreateAndAddWindow(tTJSNI_Window *w) {
	return new TVPWindowLayer(w);
}

tjs_uint32 TVPGetCurrentShiftKeyState()
{
	tjs_uint32 f = 0;

	if(TVPGetAsyncKeyState(VK_SHIFT)) f |= TVP_SS_SHIFT;
	if(TVPGetAsyncKeyState(VK_MENU)) f |= TVP_SS_ALT;
	if(TVPGetAsyncKeyState(VK_CONTROL)) f |= TVP_SS_CTRL;
	if(TVPGetAsyncKeyState(VK_LBUTTON)) f |= TVP_SS_LEFT;
	if(TVPGetAsyncKeyState(VK_RBUTTON)) f |= TVP_SS_RIGHT;
	if(TVPGetAsyncKeyState(VK_MBUTTON)) f |= TVP_SS_MIDDLE;

	return f;
}

ttstr TVPGetPlatformName()
{
	return SDL_GetPlatform();
}

ttstr TVPGetOSName()
{
	return TVPGetPlatformName();
}

tjs_uint32 TVP_TShiftState_To_uint32(TShiftState state) {
	tjs_uint32 result = 0;
	if (state & MK_SHIFT) {
		result |= ssShift;
	}
	if (state & MK_CONTROL) {
		result |= ssCtrl;
	}
	if (state & MK_ALT) {
		result |= ssAlt;
	}
	return result;
}
TShiftState TVP_TShiftState_From_uint32(tjs_uint32 state){
	TShiftState result = 0;
	if (state & ssShift) {
		result |= MK_SHIFT;
	}
	if (state & ssCtrl) {
		result |= MK_CONTROL;
	}
	if (state & ssAlt) {
		result |= MK_ALT;
	}
	return result;
}

#include <dirent.h>
extern void TVPInitializeFont();

static ttstr TVPDefaultFaceNames;
extern void TVPAddSystemFontToFreeType( const std::string& storage, std::vector<tjs_string>* faces );
extern void TVPGetSystemFontListFromFreeType( std::vector<tjs_string>& faces );
static bool TVPIsGetAllFontList = false;
static ttstr TVPDefaultFontNameX;
void TVPGetAllFontList( std::vector<tjs_string>& list ) {
	TVPInitializeFont();
	if( TVPIsGetAllFontList ) {
		TVPGetSystemFontListFromFreeType( list );
	}

	DIR* dr;
	if( ( dr = opendir("/System/Library/Fonts/") ) != nullptr ) {
		struct dirent* entry;
		while( ( entry = readdir( dr ) ) != nullptr ) {
			if( entry->d_type == DT_REG ) {
				std::string path(entry->d_name);
				std::string::size_type extp = path.find_last_of(".");
				if( extp != std::string::npos ) {
					std::string ext = path.substr(extp);
					if( ext == std::string(".ttf") || ext == std::string(".ttc") || ext == std::string(".otf") ) {
						// .ttf | .ttc | .otf
						std::string fullpath( std::string("/System/Library/Fonts/") + path );
						TVPAddSystemFontToFreeType( fullpath, &list );
					}
				}
			}
		}
		closedir( dr );
		TVPIsGetAllFontList = true;
	}
#if 0
	for( std::list<std::string>::const_iterator i = fontfiles.begin(); i != fontfiles.end(); ++i ) {
		FT_Face face = nullptr;
		std::string fullpath( std::string("/system/fonts/") + *i );
		FT_Open_Args args;
		memset(&args, 0, sizeof(args));
		args.flags = FT_OPEN_PATHNAME;
		args.pathname = fullpath.c_str();
		tjs_uint face_num = 1;
		std::list<std::string> facenames;
		for( tjs_uint f = 0; f < face_num; f++ ) {
			FT_Error err = FT_Open_Face( FreeTypeLibrary, &args, 0, &face);
			if( err == 0 ) {
				facenames.push_back( std::string(face->family_name) );
				std::string(face->style_name);	// スタイル名
				if( face->face_flags & FT_FACE_FLAG_SCALABLE ) {
					// 可変サイズフォントのみ採用
					if( face->num_glyphs > 2965 ) {
						// JIS第一水準漢字以上のグリフ数
						if( face->style_flags & FT_STYLE_FLAG_ITALIC ) {}
						if( face->style_flags & FT_STYLE_FLAG_BOLD ) {}
						face_num = face->num_faces;
						int numcharmap = face->num_charmaps;
						for( int c = 0; c < numcharmap; c++ ) {
							FT_Encoding enc = face->charmaps[c]->encoding;
							if( enc == FT_ENCODING_SJIS ) {
								// mybe japanese
							}
							if( enc == FT_ENCODING_UNICODE ) {
							}
						}
					}
				}
			}
			if(face) FT_Done_Face(face), face = nullptr;
		}
	}
#endif
}
static bool IsInitDefalutFontName = false;
static bool SelectFont( const std::vector<tjs_string>& faces, tjs_string& face ) {
	std::vector<tjs_string> fonts;
	TVPGetAllFontList( fonts );
	for( auto i = faces.begin(); i != faces.end(); ++i ) {
		auto found = std::find( fonts.begin(), fonts.end(), *i );
		if( found != fonts.end() ) {
			face = *i;
			return true;
		}
	}
	return false;
}
const tjs_char *TVPGetDefaultFontName() {
	if( IsInitDefalutFontName ) {
		return TVPDefaultFontNameX.c_str();
	}
	TVPDefaultFontNameX = TJS_W("Noto Sans CJK JP");
	IsInitDefalutFontName =  true;

	// コマンドラインで指定がある場合、そのフォントを使用する
	tTJSVariant opt;
	if(TVPGetCommandLine(TJS_W("-deffont"), &opt)) {
		ttstr str(opt);
		TVPDefaultFontNameX = str;
	} else {
		tjs_string face;
		std::vector<tjs_string> facenames{tjs_string(TJS_W("Noto Sans CJK JP"))};
		if( SelectFont( facenames, face ) ) {
			TVPDefaultFontNameX = face;
		}
	}
	return TVPDefaultFontNameX.c_str();
}
void TVPSetDefaultFontName( const tjs_char * name ) {
	TVPDefaultFontNameX = name;
}
const ttstr &TVPGetDefaultFaceNames() {
	static ttstr default_facenames = "Noto Sans,MotoyaLMaru,Roboto";
#if 0
	if( !TVPDefaultFaceNames.IsEmpty() ) {
		return TVPDefaultFaceNames;
	} else {
		TVPDefaultFaceNames = ttstr( TVPGetDefaultFontName() );
		return TVPDefaultFaceNames;
	}
#endif
	return default_facenames;
}


