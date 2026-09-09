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

// ImGui消息处理
LRESULT WINAPI FFO_ImGui_WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto &io = ImGui::GetIO();

    // 焦点是否位于ImGui文本框(InputText等)内
    const bool want_text_input = io.WantTextInput;
    // 鼠标是否悬停/交互于ImGui窗口或控件上
    const bool want_capture_mouse = io.WantCaptureMouse;

    io.MouseDrawCursor = want_capture_mouse;

    switch (msg)
    {
    // ---- 按键消息: 喂给ImGui维护按键状态(如快捷键Ctrl+S), 无DefWindowProc副作用 ----
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
        ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
        // 需求1: 不在文本框内 -> 按键同时透传给游戏
        if (!want_text_input)
        {
            return ffo_wndproc(hWnd, msg, wParam, lParam);
        }
        // 在文本框内 -> 只给ImGui
        return 0;

    // ---- 字符/输入法(IME)消息: 只送给需要文字的一方, 避免重复 ----
    case WM_CHAR:
    case WM_SYSCHAR:
    case WM_DEADCHAR:
    case WM_IME_STARTCOMPOSITION:
    case WM_IME_COMPOSITION:
    case WM_IME_ENDCOMPOSITION:
    case WM_IME_CHAR:
    case WM_IME_SETCONTEXT:
    case WM_IME_NOTIFY:
    case WM_IME_CONTROL:
    case WM_IME_KEYDOWN:
    case WM_IME_KEYUP:
    case WM_IME_SELECT:
        // 焦点在ImGui文本框内 -> 只喂给ImGui(由其内部处理合成字符), 不下发游戏
        if (want_text_input)
        {
            ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
            return 0;
        }
        // 不在文本框内 -> 直接透传给游戏, 不喂给ImGui
        // (ImGui的WM_IME_COMPOSITION分支内部会调DefWindowProc再合成一次字符, 导致游戏中文双份)
        return ffo_wndproc(hWnd, msg, wParam, lParam);

    // ---- 鼠标消息 ----
    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_MBUTTONDBLCLK:
    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP:
    case WM_XBUTTONDBLCLK:
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
    case WM_MOUSELEAVE:
        ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
        // 需求2: 鼠标在ImGui窗口上 -> 吞掉消息, 防止点击/滚动穿透到游戏界面
        if (want_capture_mouse)
        {
            return 0;
        }
        break; // 不在ImGui上 -> 继续透传给游戏

    default:
        // 其余消息(如WM_SETCURSOR)照常交给ImGui处理
        if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        {
            return 0;
        }
        break;
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
