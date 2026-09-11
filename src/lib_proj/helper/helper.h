#pragma once
#include "config.hpp"
#include "context.hpp"
#include <Windows.h>
#include <array>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <thread>
#include <vector>

// 助手（魔手）核心类：
//   - 负责配置的读写（ffohelper.json）；
//   - 负责修改游戏帧率与取得游戏窗口句柄的内存补丁；
//   - 负责绘制 ImGui 界面；
//   - 负责在后台线程中按各技能间隔自动按键。
class helper_class
{
  public:
    // 安装内存补丁：替换游戏的 Sleep 以实现帧率控制，并取得游戏窗口句柄
    void patch();
    // 开始工作：载入配置并提升定时器精度
    void begin_helper();
    // 结束工作：停止魔手线程、恢复定时器精度并保存配置
    void end_helper();

    // 取得当前选中的配置方案
    profile &get_current_profile();

    // 每帧调用，绘制 ImGui 界面
    void imgui_process();

  private:
    config                      _config;              // 全部配置数据
    context                     _context;             // 运行期状态
    std::optional<std::jthread> _magic_hand_thread;   // 魔手工作线程（可选）

    std::filesystem::path get_config_path();  // 计算配置文件路径
    void                  load_config();      // 从文件载入配置
    void                  save_config();      // 保存配置到文件
    void                  begin_magic_hand(); // 启动魔手线程
    void                  end_magic_hand();   // 停止魔手线程
    void                  magic_hand_thread_proc(std::stop_token stt); // 魔手线程主体
};

// 全局唯一的助手实例
extern helper_class helper_instance;
