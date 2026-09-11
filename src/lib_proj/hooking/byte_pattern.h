// Core code from Hooking.Patterns
// https://github.com/ThirteenAG/Hooking.Patterns

#pragma once
#include <Windows.h>
#include <stdexcept>
#include <string>
#include <vector>

// =============================================================================
//  byte_pattern —— 内存特征码搜索
//
//  用于在游戏进程中按「特征码」（形如 "56 8B CF FF 75 08 E8 ? ? ? ?"）定位代码，
//  其中 "?" 为通配符（半字节通配用 "?F"/"F?" 表示），找到地址后可结合
//  injector 的相关接口进行读写或打补丁。
//  搜索算法为 Boyer-Moore-Horspool，并通过 _bmbc 跳转表加速。
// =============================================================================

// 内嵌地址的轻量包装：支持整型地址与指针之间的相互转换及偏移计算
class memory_pointer
{
  public:
    memory_pointer() : _address(0)
    {
    }
    memory_pointer(const void *pointer) : _address(reinterpret_cast<std::intptr_t>(pointer))
    {
    }
    explicit memory_pointer(std::intptr_t address) : _address(address)
    {
    }

    // 以指定类型指针返回（可带字节偏移）
    template <typename T = void> T *p(std::intptr_t offset = 0) const
    {
        return reinterpret_cast<T *>(i(offset));
    }

    // 以整型地址返回（可带字节偏移）
    std::intptr_t i(std::intptr_t offset = 0) const
    {
        return (_address + offset);
    }

    memory_pointer &operator=(const void *pointer)
    {
        _address = reinterpret_cast<std::intptr_t>(pointer);
        return *this;
    }

    memory_pointer &operator=(std::intptr_t address)
    {
        _address = address;
        return *this;
    }

    template <typename T> operator T *() const
    {
        return p<T>();
    }

    operator std::intptr_t() const
    {
        return _address;
    }

  private:
    std::intptr_t _address;
};

// 特征码搜索器
class byte_pattern
{
    std::pair<std::intptr_t, std::intptr_t> _range;   // 搜索范围 [起始, 结束)
    std::vector<std::uint8_t>               _pattern; // 已解析的特征码字节
    std::vector<std::uint8_t>               _mask;    // 每个字节的掩码（0 表示通配）
    std::vector<memory_pointer>             _results; // 搜索结果
    std::ptrdiff_t                          _bmbc[256]; // BMH 跳转表

    static std::vector<std::string>    split_pattern(const char *literal);          // 按空格拆分特征码
    static std::pair<uint8_t, uint8_t> parse_sub_pattern(std::string_view sub);     // 解析单个字节/通配符
    void                               transform_pattern(const char *literal);      // 字符串 -> 字节+掩码

    void get_module_range(memory_pointer module); // 取模块映像的内存范围

    void bm_preprocess(); // 预处理跳转表
    void bm_search();     // 执行搜索

  public:
    byte_pattern();

    byte_pattern &set_pattern(const char *pattern_literal);       // 设置字符串形式特征码
    byte_pattern &set_pattern(const void *pattern_binary, std::size_t size); // 设置二进制特征码

    byte_pattern &reset_module();                                  // 搜索范围重置为当前模块
    byte_pattern &set_module(memory_pointer module);               // 搜索范围设为指定模块
    byte_pattern &set_range(memory_pointer beg, memory_pointer end); // 搜索范围设为指定区间

    byte_pattern &search(); // 按当前特征码执行搜索

    byte_pattern &find_pattern(const char *pattern_literal); // 设置并立即搜索
    byte_pattern &find_pattern(const void *pattern_binary, std::size_t size);

    memory_pointer              get(std::size_t index) const; // 取第 index 个结果
    std::vector<memory_pointer> get() const;                  // 取全部结果
    memory_pointer              get_first() const;            // 取第一个结果

    std::size_t count() const;                          // 结果个数
    bool        has_size(std::size_t expected) const;   // 结果个数是否等于预期
    bool        empty() const;                          // 是否没有结果
    void        clear();                                // 清空特征码与结果

    template <typename Fn> void for_each_result(Fn fn) const
    {
        for (memory_pointer p : this->_results)
        {
            fn(p);
        }
    }
};
