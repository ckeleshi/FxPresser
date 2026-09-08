#ifndef FXPRESSER_PATCH_ONLY
#include "helper/helper.h"
#include <imgui.h>
#include <imgui_impl_opengl2.h>
#include <imgui_impl_win32.h>
#endif

#include "hooking/byte_pattern.h"
#include "hooking/injector/calling.hpp"
#include "hooking/injector/hooking.hpp"
#include "hooking/injector/utility.hpp"

#include <WinUser.h>
#include <Windows.h>
#include <cstdint>

#ifndef FXPRESSER_PATCH_ONLY
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

namespace
{
#ifndef FXPRESSER_PATCH_ONLY
std::intptr_t display3d_base;
WNDPROC       ffo_wndproc;

static injector::hook_back<int(__fastcall *)(std::intptr_t, int, HWND, int)> Display3D_Init_Hookback;
static injector::hook_back<int(__fastcall *)(std::intptr_t)>                 Display3D_Update_Hookback;
static injector::hook_back<int(__fastcall *)(std::intptr_t)>                 Display3D_Destroy_Hookback;

wchar_t Param_To_WideChar(WPARAM wParam)
{
    wchar_t     wc;
    char        buffer[2];
    const char *pSrc = reinterpret_cast<const char *>(&wParam);
    buffer[0]        = pSrc[1];
    buffer[1]        = pSrc[0];
    MultiByteToWideChar(936, 0, buffer, 2, &wc, 1);
    return wc;
}

// ImGui消息处理
LRESULT WINAPI FFO_ImGui_WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    ImGuiIO &io = ImGui::GetIO();

    io.MouseDrawCursor = io.WantCaptureMouse;

    bool processed = false;

    if (io.WantCaptureKeyboard)
    {
        if (msg == WM_CHAR && wParam >= 0xA0 && lParam == 1)
        {
            // 忽略被拆开的GB2312字节
            processed = true;
        }
        else if (msg == WM_IME_CHAR && wParam > 0xA000 && lParam == 1)
        {
            // 将完整的GB2312字符转为Unicode再投给ImGui
            io.AddInputCharacterUTF16(Param_To_WideChar(wParam));
            processed = true;
        }
    }

    if (!processed)
    {
        ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
    }
    else
    {
        return 0;
    }

    if (io.WantCaptureMouse && msg == WM_LBUTTONDOWN)
    {
        return 0;
    }

    if (io.WantCaptureKeyboard && msg == WM_CHAR)
    {
        return 0;
    }

    return ffo_wndproc(hWnd, msg, wParam, lParam);
}

// ImGui初始化
int __fastcall FFO_ImGui_Init(std::intptr_t display3d, int, HWND a0, int a4)
{
    auto game_init_result = Display3D_Init_Hookback.fun(display3d, 0, a0, a4);

    if (game_init_result == 0)
    {
        ImGui::CreateContext();
        ImGui::StyleColorsDark();

        ImGui_ImplWin32_InitForOpenGL(a0);
        ImGui_ImplOpenGL2_Init();

        ImGuiIO &io = ImGui::GetIO();

        // 是的，C盘
        ImFont *font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\msyh.ttc", 18.0f, nullptr,
                                                    io.Fonts->GetGlyphRangesChineseFull());

        SetWindowLongPtrA(a0, GWLP_WNDPROC, reinterpret_cast<LONG>(&FFO_ImGui_WndProc));
    }

    return game_init_result;
}

// ImGui绘制过程
int __fastcall FFO_ImGui_Update(std::intptr_t display3d)
{
    ImGui_ImplOpenGL2_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    helper_instance.imgui_process();

    ImGui::Render();
    ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());

    return Display3D_Update_Hookback.fun(display3d);
}

// 销毁ImGui
int __fastcall FFO_ImGui_Destroy(std::intptr_t display3d)
{
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    return Display3D_Destroy_Hookback.fun(display3d);
}
#endif
} // namespace

void inject_game()
{
    byte_pattern patterner;

#ifndef FXPRESSER_PATCH_ONLY
    auto display3d_module = GetModuleHandleW(L"Display3D.dll");
    display3d_base        = reinterpret_cast<std::intptr_t>(display3d_module);
    auto display3d_vtbl = reinterpret_cast<std::intptr_t>(GetProcAddress(display3d_module, "??_7IDisplay@@6B@")) + 0xDC;

    // 插入ImGui渲染
    injector::ReadObject(display3d_vtbl + 0xC, Display3D_Init_Hookback.fun);
    injector::WriteObject(display3d_vtbl + 0xC, &FFO_ImGui_Init, true);
    injector::WriteObject(display3d_vtbl + 0x10, &FFO_ImGui_Init, true);

    injector::ReadObject(display3d_vtbl + 0x24, Display3D_Update_Hookback.fun);
    injector::WriteObject(display3d_vtbl + 0x24, &FFO_ImGui_Update, true);

    injector::ReadObject(display3d_vtbl + 0x18, Display3D_Destroy_Hookback.fun);
    injector::WriteObject(display3d_vtbl + 0x18, &FFO_ImGui_Destroy, true);
    
    // 储存原始WndProc函数
    patterner.find_pattern("C7 45 A8 08 00 00 00 C7 45 AC");
    if (patterner.has_size(1))
    {
        ffo_wndproc = injector::ReadMemory<WNDPROC>(patterner.get(0).i(10));
    }
#endif

    // Patch导致PostMessage失效的地方
    patterner.find_pattern("56 8B CF FF 75 08 E8 ? ? ? ? 84 C0 75 14");
    if (patterner.has_size(1))
    {
        injector::MakeNOP(patterner.get(0).i(13), 2);
    }

    patterner.find_pattern("80 B9 0C 01 00 00 00 75 1E");
    if (patterner.has_size(1))
    {
        injector::WriteObject<unsigned char>(patterner.get(0).i(7), 0xEBu, true);
    }
}
