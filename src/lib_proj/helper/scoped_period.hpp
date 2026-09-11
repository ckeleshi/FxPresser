#pragma once
#include <Windows.h>
#include <cstring>


// RAII 方式管理多媒体定时器精度：
// 构造时把系统最小定时器精度提升到设备支持的最小值（通常 1ms），
// 析构时自动恢复，保证 Sleep/等待循环的精度足够用于按键与帧率控制。
class scoped_period
{
  public:
    scoped_period()
    {
        std::memset(&_caps, 0, sizeof(_caps));
        timeGetDevCaps(&_caps, sizeof(_caps)); // 查询设备支持的精度范围
        timeBeginPeriod(_caps.wPeriodMin);    // 提升全局定时器精度
    }

    ~scoped_period()
    {
        timeEndPeriod(_caps.wPeriodMin); // 恢复原有精度
    }

  private:
    TIMECAPS _caps; // 设备支持的定时器精度信息
};
