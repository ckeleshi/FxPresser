#pragma once
#include "char8_t-remediation.h"
#include <array>
#include <nlohmann/json.hpp>
#include <string>

// 单套配置方案：保存 F1~F10 的启用状态、间隔、耗时与缺省标记，以及目标帧率。
// 通过 NLOHMANN_DEFINE_TYPE_INTRUSIVE 自动生成 JSON 序列化/反序列化。
struct profile
{
    std::string            name;                          // 方案名称
    int                    fps = 60;                      // 目标帧率
    std::array<bool, 10>   magic_hand_key_enable_flags;   // 各技能是否启用
    std::array<double, 10> magic_hand_key_intervals;      // 各技能连续施放间隔(秒)
    std::array<double, 10> magic_hand_key_latencies;      // 各技能施放后占用的时间(秒)
    std::array<bool, 10>   magic_hand_key_is_default_flags; // 是否为缺省技能(每轮只按一次)

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(profile, name, fps, magic_hand_key_enable_flags, magic_hand_key_intervals,
                                   magic_hand_key_latencies, magic_hand_key_is_default_flags)

    // 默认值：名称为「默认」，全部技能关闭，间隔 1.0 秒，耗时 0.8 秒
    profile()
    {
        name = U8("默认");
        magic_hand_key_enable_flags.fill(false);
        magic_hand_key_intervals.fill(1.0);
        magic_hand_key_latencies.fill(0.8);
        magic_hand_key_is_default_flags.fill(false);
    }
};
