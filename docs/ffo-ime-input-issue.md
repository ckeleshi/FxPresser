# FFO_ImGui_WndProc 中文输入问题分析

> 日期：2026-09-08
> 范围：`src/lib_proj/ffo.cpp` 的 `FFO_ImGui_WndProc`
> 结论：**在 ImGui 界面之外（游戏自身聊天框）打汉字，很可能被这段逻辑吞掉。**

---

## 1. 现象

- 在游戏自己的聊天输入框里**无法输入汉字 / 输入无效**。
- ASCII、英文通常正常（走 `WM_CHAR` 时可透传）。

## 2. 根因

`ffo.cpp` 中存在一个「无条件把消息交给 ImGui 后端，后端返回非 0 就直接吞掉」的分支：

```cpp
if (!processed)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
    {
        return true;          // 消息被吞，不再转给游戏 ffo_wndproc
    }
}
```

而项目使用的 ImGui 是 **v1.92.8**
（源码在 `vcpkg/buildtrees/imgui/src/v1.92.8-8e9c05eb59.clean/backends/imgui_impl_win32.cpp`）。

该后端对 IME 消息**无条件返回 1（视为已处理）**，与 `io.WantCaptureKeyboard` 无关：

```cpp
case WM_IME_CHAR:
    if (::IsWindowUnicode(hwnd) == FALSE)      // ANSI 窗口时
    {
        ...
        io.AddInputCharacterUTF16(wch);
        return 1;                              // 直接消费，返回“已处理”
    }
    return 0;

case WM_IME_COMPOSITION:
    LRESULT result = ::DefWindowProcW(hwnd, msg, wParam, lParam);
    return (lParam & GCS_RESULTSTR) ? 1 : result;   // 提交结果时也返回 1
```

因此：

- `WM_IME_CHAR`（ANSI/GBK 游戏拿到中文的主要通道）→ 返回 1 → 被吞，游戏收不到；
- `WM_IME_COMPOSITION`（带 `GCS_RESULTSTR`，IME 最终提交）→ 返回 1 → 被吞，游戏读不到提交结果。

> 本游戏窗口为 ANSI（GB2312/GBK，代码页 936，`ffo.cpp` 内全部按此处理），
> IME 正是靠上述两类消息把汉字交给窗口，一旦被后端“认领”，原始 `ffo_wndproc`
> 永远收不到。

## 3. 为什么已有的 `WantCaptureKeyboard` 拦截不够

`ffo.cpp:51-63`、`:83-85` 的门控都用了 `io.WantCaptureKeyboard`，但它们只保护
自己写的 `WM_CHAR / WM_IME_CHAR` 分支；

`ffo.cpp:66-70` 调用 ImGui 后端那句是**无条件执行**的，而该后端在 IME 消息上
返回 1 **并不依赖 `WantCaptureKeyboard`**，所以照样把「未在 ImGui 内输入的」
IME 消息吞掉。

## 4. 建议修复方向

原则：**只有 ImGui 真正需要键盘时才允许后端消费字符/IME 消息；其余情况
IME/字符类消息直接放给游戏。**

示意伪代码：

```cpp
LRESULT WINAPI FFO_ImGui_WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    ImGuiIO &io = ImGui::GetIO();
    io.MouseDrawCursor = io.WantCaptureMouse;

    const bool ime_or_char = (msg == WM_CHAR || msg == WM_IME_CHAR ||
                              msg == WM_IME_STARTCOMPOSITION || msg == WM_IME_COMPOSITION ||
                              msg == WM_IME_ENDCOMPOSITION);

    // 在 ImGui 输入框里打字时才处理汉字转换
    if (io.WantCaptureKeyboard && ime_or_char)
    {
        bool processed = false;
        if (msg == WM_CHAR && wParam >= 0xA0 && lParam == 1)
            processed = true;                       // 忽略被拆开的 GB2312 字节
        else if (msg == WM_IME_CHAR && wParam > 0xA000 && lParam == 1)
        {
            io.AddInputCharacterUTF16(Param_To_WideChar(wParam));
            processed = true;
        }
        if (processed)
            return 0;
    }
    else if (!io.WantCaptureKeyboard && ime_or_char)
    {
        // ImGui 不抢键盘：字符/IME 消息一律透传给游戏，避免被后端吞掉
        return ffo_wndproc(hWnd, msg, wParam, lParam);
    }

    // 鼠标 / 普通按键仍喂给 ImGui 后端；后端“已处理”才吞
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    if (io.WantCaptureMouse && msg == WM_LBUTTONDOWN)
        return 0;

    if (io.WantCaptureKeyboard && msg == WM_CHAR)
        return 0;

    // ...F1~F10 / Alt+数字 等原有逻辑保持不变...
}
```

## 5. 待确认事项

- [ ] 确认游戏主窗口是否确实是 ANSI（非 Unicode）窗口类。
      （从代码按 GB2312/936 处理来看基本可断定，但仍建议运行时用
      `IsWindowUnicode()` 验证。）
- [ ] 确认游戏聊天输入框的消息是走**被 hook 的主窗口 WndProc**，
      还是走某个独立子控件（IME 消息走子控件时不会被本逻辑拦截，
      则问题不在此处）。
