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

// =============================================================================
//  游戏注入实现
//
//  主要工作：
//   1. 通过 Hook Display3D.dll 的虚表（IDisplay 的 Init/Update/Destroy），
//      在游戏渲染流程中插入 ImGui 的初始化、绘制与销毁；
//   2. 接管游戏窗口过程（WndProc），把消息同时分发给 ImGui 与游戏，
//      并避免鼠标/键盘事件重复穿透；
//   3. 用特征码定位并 NOP 掉导致 PostMessage 失效的代码，使外置按键生效。
//  定义 FXPRESSER_PATCH_ONLY 时只保留第 3 项（补丁版，不含 ImGui 界面）。
// =============================================================================

#ifndef FXPRESSER_PATCH_ONLY
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

namespace
{
#ifndef FXPRESSER_PATCH_ONLY
// Display3D.dll 基址；游戏原始窗口过程；以及被 Hook 的三个虚函数的原始入口
std::intptr_t display3d_base;
WNDPROC       ffo_wndproc;

static injector::hook_back<int(__fastcall *)(std::intptr_t, int, HWND, int)> Display3D_Init_Hookback;
static injector::hook_back<int(__fastcall *)(std::intptr_t)>                 Display3D_Update_Hookback;
static injector::hook_back<int(__fastcall *)(std::intptr_t)>                 Display3D_Destroy_Hookback;

// ImGui消息处理：在游戏窗口过程中穿插 ImGui 的消息处理
// 规则：
//   - 键盘消息：不在 ImGui 文本框内时透传给游戏；在文本框内则只交给 ImGui；
//   - 字符/IME 消息：只送给真正需要的一方，避免中文输入被合成两次；
//   - 鼠标消息：鼠标位于 ImGui 窗口上时吞掉，防止点击穿透到游戏。
LRESULT WINAPI FFO_ImGui_WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto &io = ImGui::GetIO();

    // 焦点是否位于ImGui文本框(InputText等)内
    const bool want_text_input    = io.WantTextInput;
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

// ImGui初始化：在游戏 Display3D 初始化成功后创建 ImGui 上下文，并替换窗口过程
int __fastcall FFO_ImGui_Init(std::intptr_t display3d, int, HWND a0, int a4)
{
    auto game_init_result = Display3D_Init_Hookback.fun(display3d, 0, a0, a4);

    // 仅在游戏自身初始化成功时初始化 ImGui
    if (game_init_result == 0)
    {
        ImGui::CreateContext();
        ImGui::StyleColorsDark();

        // 分别对接 Win32 与 OpenGL2 后端（FFO 使用固定管线 OpenGL）
        ImGui_ImplWin32_InitForOpenGL(a0);
        ImGui_ImplOpenGL2_Init();

        ImGuiIO &io = ImGui::GetIO();

        // 是的，C盘
        // 载入微软雅黑字体，并包含完整中文字形
        ImFont *font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\msyh.ttc", 18.0f, nullptr,
                                                    io.Fonts->GetGlyphRangesChineseFull());

        // 用自身窗口过程替换游戏原窗口过程，以便插入消息处理
        SetWindowLongPtrA(a0, GWLP_WNDPROC, reinterpret_cast<LONG>(&FFO_ImGui_WndProc));
    }

    return game_init_result;
}

// ImGui绘制过程：每帧先构建并渲染 ImGui 界面，再调用游戏原本的绘制逻辑
int __fastcall FFO_ImGui_Update(std::intptr_t display3d)
{
    ImGui_ImplOpenGL2_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // 由助手绘制具体界面（FFOHelper 窗口）
    helper_instance.imgui_process();

    ImGui::Render();
    ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());

    return Display3D_Update_Hookback.fun(display3d);
}

// 销毁ImGui：在游戏销毁显示对象前，先释放 ImGui 相关资源
int __fastcall FFO_ImGui_Destroy(std::intptr_t display3d)
{
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    return Display3D_Destroy_Hookback.fun(display3d);
}
#endif
} // namespace

DWORD WINAPI GetChronoTickCount()
{
    using namespace std::chrono;

    return static_cast<DWORD>(
        duration_cast<milliseconds>(high_resolution_clock::now() - high_resolution_clock::time_point()).count());
}

// 注入游戏：安装 ImGui 相关 Hook、保存原窗口过程，并修补阻止 PostMessage 生效的代码
void inject_game()
{
    byte_pattern patterner;

#ifndef FXPRESSER_PATCH_ONLY
    // 取得 Display3D.dll 基址及其 IDisplay 虚表地址（虚表 + 0xDC 处为各虚函数指针）
    auto display3d_module = GetModuleHandleW(L"Display3D.dll");
    display3d_base        = reinterpret_cast<std::intptr_t>(display3d_module);
    auto display3d_vtbl = reinterpret_cast<std::intptr_t>(GetProcAddress(display3d_module, "??_7IDisplay@@6B@")) + 0xDC;

    // 插入ImGui渲染：替换虚表中的 Init/Update/Destroy 三个函数指针，
    // 同时把原函数地址记录到 Hookback，供新函数内部回调原逻辑。
    injector::ReadObject(display3d_vtbl + 0xC, Display3D_Init_Hookback.fun);
    injector::WriteObject(display3d_vtbl + 0xC, &FFO_ImGui_Init, true);
    injector::WriteObject(display3d_vtbl + 0x10, &FFO_ImGui_Init, true);

    injector::ReadObject(display3d_vtbl + 0x24, Display3D_Update_Hookback.fun);
    injector::WriteObject(display3d_vtbl + 0x24, &FFO_ImGui_Update, true);

    injector::ReadObject(display3d_vtbl + 0x18, Display3D_Destroy_Hookback.fun);
    injector::WriteObject(display3d_vtbl + 0x18, &FFO_ImGui_Destroy, true);

    // 储存原始WndProc函数：从特征码附近的内存中读出游戏原始窗口过程指针
    patterner.find_pattern("C7 45 A8 08 00 00 00 C7 45 AC");
    if (patterner.has_size(1))
    {
        ffo_wndproc = injector::ReadMemory<WNDPROC>(patterner.get(0).i(10));
    }
#endif

    // Patch导致PostMessage失效的地方
    // 游戏内部有一段代码会在收到按键消息后跳过处理，导致外置 PostMessage 无效，
    // 这里用 NOP 覆盖该条件跳转，使外部投递的按键能被正常处理。
    patterner.find_pattern("56 8B CF FF 75 08 E8 ? ? ? ? 84 C0 75 14");
    if (patterner.has_size(1))
    {
        injector::MakeNOP(patterner.get(0).i(13), 2);
    }
}
