#pragma once
#include "target_clock.hpp"
#include <array>

// 魔手运行期的临时状态（不写入配置文件）
struct context
{
    bool magic_hand_global_switch = false; // 全局开关是否打开
    bool magic_hand_expanded      = false; // 界面是否处于展开状态

    target_clock                 magic_hand_global_clock;         // 全局按键间隔时钟
    std::array<target_clock, 10> magic_hand_key_clocks;           // 各技能自身的间隔时钟
    std::array<bool, 10>         magic_hand_default_key_pressed; // 缺省技能本次是否已触发过
};
