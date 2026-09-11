#pragma once
#include <chrono>

// 目标时刻时钟：维护一个「下次可触发时刻」，用于按键间隔与帧率控制。
// 每次触发后通过 adjust_target_time 向后顺延，而不是重新从当前时间起算，
// 这样可以避免累积误差导致实际间隔逐渐变长。
class target_clock
{
  public:
    // 直接指定目标时刻
    void set_target_time(std::chrono::steady_clock::time_point tp)
    {
        _target_time = tp;
    }

    // 以「当前时刻 + 周期」作为目标时刻
    void set_target_time_from_now(std::chrono::steady_clock::duration period)
    {
        _target_time = std::chrono::steady_clock::now() + period;
    }

    // 目标时刻是否已到达
    bool target_time_reached() const
    {
        return std::chrono::steady_clock::now() >= _target_time;
    }

    // 在目标时刻基础上再向后顺延一段时间（用于下一次触发）
    void adjust_target_time(std::chrono::steady_clock::duration additional)
    {
        _target_time += additional;
    }

    // 距目标时刻还剩多少时间（可为负数，表示已超时）
    std::chrono::steady_clock::duration rest_to_target_time() const
    {
        return _target_time - std::chrono::steady_clock::now();
    }

  private:
    std::chrono::steady_clock::time_point _target_time; // 目标时刻
};
