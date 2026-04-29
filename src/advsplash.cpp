/***************************************************
 * FILE NAME: advsplash.cpp
 *
 * Copyright 2003 - Present NSIS
 *
 * PURPOSE:
 *    Splash screen in NSIS installers with fading
 *    and transparency effects
 *
 * CHANGE HISTORY
 *
 * Author              Date          Modifications
 *
 * Justin                            Original
 * Amir Szekely (kichik)      Converted to a plugin DLL
 * Nik Medved (brainsucker)   Fading and transparency by
 * Takhir Bedertdinov
 *    10-26-2004  Gif and jpeg support
 *    06-25-2005  modeless mode
 *    08-30-2005  hwnd entry point for AnimGif
 *    10-16-2005  "no transparency" bug for white bg images fixed
 *    03-04-2006  /PASSIVE mode
 *    04-04-2006  NULL parent HWND for /BANNER mode
 * hbatista
 *    04-05-2007  Unregister window on 'stop'
 * Takhir
 *    11-20-2007  "stop /fadeout" option - decreases sleep time
 *                "stop /wait" fully replaces ::wait call
 **************************************************/

// For layered windows
#define _WIN32_WINNT 0x0500

#include <windows.h>
#include <tchar.h>
#include <windowsx.h>
#include <olectl.h>
#include <stdio.h>
#include <vfw.h>
#include "exdll.h"

#ifndef WS_EX_LAYERED
#define WS_EX_LAYERED 0x00080000
#define LWA_COLORKEY 0x00000001
#define LWA_ALPHA 0x00000002
#endif // ndef WS_EX_LAYERED

#define RESOLUTION 32 // 32 ms ~ 30 fps ;) (32? I like SHR more than iDIV ;)
#define MAX_SHOW_TIME 60000

enum FADE_STATES
{
   FADE_IN,
   FADE_SLEEP,
   FADE_OUT
};

const TCHAR classname[4] = _T("_sp");
HWND myWnd = NULL;
HINSTANCE g_hInstance;
HANDLE hThread = NULL;
BITMAP bm;
HBITMAP g_hbm;
int resolution = RESOLUTION;
int sleep_val, fadein_val, fadeout_val, state, timeleft, keycolor, alphaparam;
LPPICTURE pIPicture = NULL;
bool modal = true, nocancel = false, setfg = true;
UINT timerEvent;
static WNDCLASS wc;

typedef BOOL(_stdcall *_tSetLayeredWindowAttributesProc)(HWND hwnd,      // handle to the layered window
                                                         COLORREF crKey, // specifies the color key
                                                         BYTE bAlpha,    // value for the blend function
                                                         DWORD dwFlags   // action
);
_tSetLayeredWindowAttributesProc SetLayeredWindowAttributesProc;

int getTransparencyColor(TCHAR *fn_src);

void sf(HWND hw)
{
   DWORD ctid = GetCurrentThreadId();
   DWORD ftid = GetWindowThreadProcessId(GetForegroundWindow(), NULL);
   AttachThreadInput(ftid, ctid, TRUE);
   SetForegroundWindow(hw);
   AttachThreadInput(ftid, ctid, FALSE);
}

/*****************************************************
 * FUNCTION NAME: WndProc()
 * PURPOSE:
 *    window proc.
 * SPECIAL CONSIDERATIONS:
 *    for paint purposes mainly
 *****************************************************/
static LRESULT CALLBACK WndProc(HWND hwnd,
                                UINT uMsg,
                                WPARAM wParam,
                                LPARAM lParam)
{
   PAINTSTRUCT ps;
   RECT r;
   long w, h;

   switch (uMsg)
   {
   case WM_CREATE:
      SystemParametersInfo(SPI_GETWORKAREA, 0, &r, 0);
      SetWindowLong(hwnd, GWL_STYLE, 0);
      SetWindowPos(hwnd, NULL,
                   r.left + (r.right - r.left - bm.bmWidth) / 2,
                   r.top + (r.bottom - r.top - bm.bmHeight) / 2,
                   bm.bmWidth, bm.bmHeight,
                   SWP_NOZORDER | SWP_SHOWWINDOW);
      return 0;

   case WM_PAINT:
      BeginPaint(hwnd, &ps);
      if (pIPicture != NULL)
      {
         GetClientRect(hwnd, &r);
         pIPicture->get_Width(&w);
         pIPicture->get_Height(&h);
         pIPicture->Render(ps.hdc, r.left,
                           r.top, r.right - r.left, r.bottom - r.top, 0, h, w, -h, &r);
      }
      EndPaint(hwnd, &ps);

   case WM_CLOSE:
      return 0;

   case WM_LBUTTONDOWN:
      if (nocancel)
         break;
   case WM_TIMER:
      timeKillEvent(timerEvent);
      if (setfg)
         sf(hwnd);
      PlaySound(0, 0, 0);
      DestroyWindow(hwnd);
      break;
   }
   return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

/*****************************************************
 * FUNCTION NAME: SetTransparentRegion()
 * PURPOSE:
 *    creates window region
 * SPECIAL CONSIDERATIONS:
 *    kind of skin creation from bmp
 *****************************************************/
void SetTransparentRegion(HWND myWnd)
{
   HDC dc;
   int x, y;
   HRGN region, cutrgn;
   BITMAPINFO bmi = {{sizeof(BITMAPINFOHEADER), bm.bmWidth, bm.bmHeight, 1, 32, BI_RGB, 0, 0, 0, 0, 0}};
   int size = bm.bmWidth * bm.bmHeight * 4;
   int *pbmp = (int *)GlobalAlloc(GPTR, size),
       *bmp;

   bmp = pbmp;
   dc = CreateCompatibleDC(NULL);
   SelectObject(dc, g_hbm);

   x = GetDIBits(dc, g_hbm, 0, bm.bmHeight, bmp, &bmi, DIB_RGB_COLORS);
   region = CreateRectRgn(0, 0, bm.bmWidth, bm.bmHeight);

   // Search for transparent pixels
   for (y = bm.bmHeight - 1; y >= 0; y--)
      for (x = 0; x < bm.bmWidth;)
         if ((*bmp & 0xFFFFFF) == keycolor)
         {
            int j = x;
            while ((x < bm.bmWidth) && ((*bmp & 0xFFFFFF) == keycolor))
               bmp++, x++;

            // Cut transparent pixels from the original region
            cutrgn = CreateRectRgn(j, y, x, y + 1);
            CombineRgn(region, region, cutrgn, RGN_XOR);
            DeleteObject(cutrgn);
         }
         else
            bmp++, x++;

   // Set resulting region.
   SetWindowRgn(myWnd, region, TRUE);
   DeleteObject(region);
   DeleteObject(dc);
   GlobalFree(pbmp);
}

/*****************************************************
 * FUNCTION NAME: TimeProc()
 * PURPOSE:
 *    fade in/out tracking
 * SPECIAL CONSIDERATIONS:
 *    works without WM_TIMER
 *****************************************************/
void CALLBACK TimeProc(UINT uID,
                       UINT uMsg,
                       DWORD_PTR dwUser,
                       DWORD_PTR dw1,
                       DWORD_PTR dw2)
{
   int call = -1;

   switch (state)
   {
   case FADE_IN:
      if (timeleft == 0)
      {
         timeleft = sleep_val;
         state++;
         if (SetLayeredWindowAttributesProc != NULL)
            call = 255;
      }
      else
      {
         call = ((fadein_val - timeleft) * 255) / fadein_val;
         break;
      }
   case FADE_SLEEP:
      if (timeleft == 0)
      {
         timeleft = fadeout_val;
         state++;
      }
      else
         break;
   case FADE_OUT:
      if (timeleft == 0)
      {
         PostMessage((HWND)dwUser, WM_TIMER, 0, 0);
         return;
      }
      else
      {
         call = ((timeleft) * 255) / fadeout_val;
         break;
      }
   }
   // Transparency value aquired, and could be set...
   if ((call >= 0) && SetLayeredWindowAttributesProc != NULL)
      SetLayeredWindowAttributesProc((HWND)dwUser, keycolor,
                                     call,
                                     alphaparam);
   // Time is running out...
   timeleft--;
}

/*****************************************************
 * FUNCTION NAME: play()
 * PURPOSE:
 *    PlaySound dll entry point
 * SPECIAL CONSIDERATIONS:
 *    makes PlaySound working on Win98
 *    works with /NOUNLOAD parameter only
 *    wav files are huge :-(
 *    but not flushes on the screen like mci window does
 *    Use it BEFORE 'show' call
 *****************************************************/
extern "C" void __declspec(dllexport) play(HWND hwndParent,
                                           int string_size,
                                           TCHAR *variables,
                                           stack_t **stacktop)
{
   TCHAR fn[MAX_PATH];
   DWORD snd = SND_FILENAME | SND_ASYNC | SND_NODEFAULT;
   EXDLL_INIT();
   if (popstring(fn) == 0)
   {
      if (lstrcmpi(fn, _T("/loop")) == 0)
      {
         snd |= SND_LOOP;
         *fn = 0;
         popstring(fn);
      }
      if (*fn == 0)
      {
         PlaySound(NULL, NULL, 0);
      }
      else
      {
         PlaySound(fn, NULL, snd);
      }
   }
}

/*****************************************************
 * FUNCTION NAME: splashThread()
 * PURPOSE:
 *    modal/modeless banner presentation
 * SPECIAL CONSIDERATIONS:
 *    from CreateThread runs window "detached", with NULL
 *    parent, otherwise uses current page as parent
 *****************************************************/
DWORD WINAPI splashThread(LPVOID lpParameter)
{
   MSG msg;
   HWND hParent = (HWND)lpParameter;
   wc.lpfnWndProc = WndProc;
   wc.hInstance = g_hInstance;
   wc.hCursor = LoadCursor(NULL, IDC_ARROW);
   wc.lpszClassName = classname;

   if (RegisterClass(&wc))
   {
      pIPicture->get_Handle((OLE_HANDLE *)&g_hbm);
      GetObject((HGDIOBJ)g_hbm, sizeof(BITMAP), &bm);

      myWnd = CreateWindowEx(WS_EX_TOOLWINDOW | (setfg ? WS_EX_TOPMOST : 0) |
                                 (SetLayeredWindowAttributesProc != NULL ? WS_EX_LAYERED : 0),
                             classname, classname, 0, 0, 0, 0, 0,
                             hParent, NULL, g_hInstance, NULL);
      // Set transparency / key color
      if (SetLayeredWindowAttributesProc != NULL)
         SetLayeredWindowAttributesProc(myWnd, keycolor,
                                        fadein_val > 0 ? 0 : 255,
                                        alphaparam);
      if (keycolor != -1)
         SetTransparentRegion(myWnd);

      // Start up timer...
      state = FADE_IN;
      timeleft = fadein_val;
      timerEvent = timeSetEvent(resolution, RESOLUTION / 4, TimeProc,
                                (DWORD_PTR)myWnd, TIME_PERIODIC);

      while (IsWindow(myWnd) && GetMessage(&msg, myWnd, 0, 0))
      {
         TranslateMessage(&msg);
         DispatchMessage(&msg);
      }

      pIPicture->Release();
      pIPicture = NULL; // do not re-use with /NOUNLOAD
   }

   myWnd = NULL;
   // Stop currently playing wave, we want to exit
   PlaySound(0, 0, 0);

   return 0;
}

/*****************************************************
 * FUNCTION NAME: show()
 * PURPOSE:
 *    image banner "show" dll entry point
 * SPECIAL CONSIDERATIONS:
 *
 *****************************************************/
extern "C" void __declspec(dllexport) show(HWND hwndParent,
                                           int string_size,
                                           TCHAR *variables,
                                           stack_t **stacktop)
{
   DEVMODE dm;
   TCHAR fn[MAX_PATH];
   WCHAR wcPort[MAX_PATH];
   DWORD dwThreadId;
   DWORD dwMainThreadId = GetCurrentThreadId();

   EXDLL_INIT();

   // 3 fade parameters and keycolor from stack come first
   popstring(fn);
   sleep_val = _tcstol(fn, NULL, 0);
   popstring(fn);
   fadein_val = _tcstol(fn, NULL, 0);
   popstring(fn);
   fadeout_val = _tcstol(fn, NULL, 0);
   popstring(fn);
   keycolor = _tcstol(fn, NULL, 0);

   // other parameters (if any) + filename
   while (!popstring(fn) && fn[0] == _T('/'))
   {
      if (lstrcmpi(fn, _T("/nocancel")) == 0)
         nocancel = true;
      else if (lstrcmpi(fn, _T("/passive")) == 0)
         setfg = false;
      else
         modal = false;
   }

   if (keycolor == -2)
      keycolor = getTransparencyColor(fn);

   // Check for winXP/2k at 32 bpp transparency support
   dm.dmSize = sizeof(DEVMODE);
   EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &dm);
   if (dm.dmBitsPerPel >= 32)
   {
      HANDLE user32 = GetModuleHandle(_T("user32"));
      SetLayeredWindowAttributesProc = (_tSetLayeredWindowAttributesProc)
          GetProcAddress((HINSTANCE)user32, "SetLayeredWindowAttributes");
   }

   if (SetLayeredWindowAttributesProc == NULL)
   {
      // Fading+transparency is unsupported at old windows versions...
      resolution = (sleep_val + fadein_val + fadeout_val) / 2;
      fadeout_val = fadein_val = 0;
      sleep_val = 1;
   }
   else
   {
      // div them by resolution - now times are in 32 ms "frames"
      sleep_val >>= 5;
      fadein_val >>= 5;
      fadeout_val >>= 5;
      // MS RGB - BGR format
      alphaparam = LWA_ALPHA | (keycolor == -1 ? 0 : LWA_COLORKEY);
      if (keycolor != -1)
         keycolor = ((keycolor & 0xFF) << 16) + (keycolor & 0xFF00) +
                    ((keycolor & 0xFF0000) >> 16);
   }

// who wrote this script :-))
#ifdef _UNICODE
   lstrcpy(wcPort, fn);
#else
   MultiByteToWideChar(CP_ACP, 0, fn, -1, wcPort, sizeof(wcPort) / 2);
#endif
   OleLoadPicturePath((LPOLESTR)wcPort, 0, 0, 0, IID_IPicture, (void **)&pIPicture);
   if (pIPicture == NULL ||
       (sleep_val + fadein_val + fadeout_val) <= 0)
      return;

   if (modal)
   {
      splashThread((void *)hwndParent);
      // We should UnRegister class, since Windows NT series never does this by itself
      UnregisterClass(wc.lpszClassName, g_hInstance);
   }
   else
   {
      // start thread and wait for window initialization
      hThread = CreateThread(0, 0, splashThread, NULL, 0, &dwThreadId);
      while (myWnd == NULL || !IsWindowVisible(myWnd))
         Sleep(10);
      if (setfg)
         sf(myWnd);
   }
}

/*****************************************************
 * FUNCTION NAME: stop()
 * PURPOSE:
 *    image banner "stop" dll entry point
 * SPECIAL CONSIDERATIONS:
 *
 *****************************************************/
extern "C" void __declspec(dllexport) stop(HWND hwndParent,
                                           int string_size,
                                           TCHAR *variables,
                                           stack_t **stacktop)
{
   TCHAR cmd[64];
   if (!modal && hThread != NULL)
   {
      if (popstring(cmd) == 0)
      {
         if (lstrcmpi(cmd, _T("/fadeout")) == 0)
         {
            if (SetLayeredWindowAttributesProc == NULL)
               stop(hwndParent, string_size, variables, stacktop);
            else
            {
               if (state == FADE_IN)
               {
                  sleep_val = 1; // minimize sleep step - 1 frame only
               }
               else if (state == FADE_SLEEP)
               {
                  timeleft = 1; // force sleep to finish
               }
               WaitForSingleObject(hThread, (fadein_val + fadeout_val + 3) * resolution);
            }
         }
         if (lstrcmpi(cmd, _T("/wait")) == 0)
         {
            WaitForSingleObject(hThread,
                                resolution * (SetLayeredWindowAttributesProc == NULL ? 2 : fadein_val + fadeout_val + sleep_val + 3));
         }
         else
            pushstring(cmd); // this is not our string
      }
      if (IsWindow(myWnd))
      {
         SendMessage(myWnd, WM_TIMER, 0, 0); // stops sound and destroys window
      }
      WaitForSingleObject(hThread, 1000);
      CloseHandle(hThread);
      hThread = NULL;

      // hbatista
      UnregisterClass(wc.lpszClassName, g_hInstance);
      Sleep(10);
      if (setfg)
         sf(hwndParent);
   }
}

/*****************************************************
 * FUNCTION NAME: hwnd()
 * PURPOSE:
 *    retrieves target window handle
 * SPECIAL CONSIDERATIONS:
 *
 *****************************************************/
extern "C" void __declspec(dllexport) hwnd(HWND hwndParent,
                                           int string_size,
                                           TCHAR *variables,
                                           stack_t **stacktop)
{
   TCHAR s[16] = _T("");
   if (IsWindow(myWnd))
      wsprintf(s, _T("%d"), myWnd);
   pushstring(s);
}

/*****************************************************
 * FUNCTION NAME: DllMain()
 * PURPOSE:
 *    Dll main (initialization) entry point
 * SPECIAL CONSIDERATIONS:
 *
 *****************************************************/
BOOL WINAPI DllMain(HANDLE hInst,
                    ULONG ul_reason_for_call,
                    LPVOID lpReserved)
{
   g_hInstance = (HINSTANCE)hInst;
   return TRUE;
}
