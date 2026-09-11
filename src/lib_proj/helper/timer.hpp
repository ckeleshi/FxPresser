#pragma once
#include <chrono>

// 简易计时器：记录起点，并查询自此经过的毫秒数
class timer
{
  public:
    // 以当前时刻作为计时起点
    void start_from_now()
    {
        _start_time = std::chrono::steady_clock::now();
    }

    // 返回自起点以来经过的毫秒数
    long long get_elapsed_ms() const
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - _start_time)
            .count();
    }

  private:
    std::chrono::steady_clock::time_point _start_time; // 计时起点
};
