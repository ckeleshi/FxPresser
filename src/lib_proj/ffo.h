#pragma once

// 对游戏进程执行初始化：加载内存补丁并安装各项 Hook。
// 由 dll.cpp 的 DllMain 在 DLL_PROCESS_ATTACH 时调用。
void inject_game();
