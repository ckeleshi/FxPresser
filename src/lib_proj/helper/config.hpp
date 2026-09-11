#pragma once
#include "profile.hpp"
#include <cstddef>
#include <nlohmann/json.hpp>
#include <vector>

// 顶层配置：包含多套配置方案(profile)，以及当前选中的方案下标。
// 通过 NLOHMANN_DEFINE_TYPE_INTRUSIVE 自动生成 JSON 序列化/反序列化。
struct config
{
    std::size_t          profile_index; // 当前配置方案下标
    std::vector<profile> profiles;      // 全部配置方案

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(config, profile_index, profiles)

    // 取得当前选中的配置方案
    profile &get_current_profile()
    {
        return profiles[profile_index];
    }

    // 修正非法配置：至少保证有一套方案，且下标在有效范围内
    void ensure_valid_config()
    {
        if (profiles.empty())
        {
            profiles.emplace_back();
        }

        if (profile_index >= profiles.size())
        {
            profile_index = 0;
        }
    }
};
