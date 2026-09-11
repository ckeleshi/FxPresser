// Core code from Hooking.Patterns
// https://github.com/ThirteenAG/Hooking.Patterns

#include "byte_pattern.h"

using namespace std;

// 取得第 index 个搜索结果；越界返回空指针
memory_pointer byte_pattern::get(size_t index) const
{
    if (this->_results.size() < (index + 1))
    {
        return nullptr;
    }
    else
    {
        return this->_results.at(index);
    }
}

// 取得全部搜索结果
std::vector<memory_pointer> byte_pattern::get() const
{
    return _results;
}

// 取得第一个搜索结果
memory_pointer byte_pattern::get_first() const
{
    return this->get(0);
}

// 构造：默认搜索范围为本模块的映像
byte_pattern::byte_pattern()
{
    reset_module();
}

// 设置字符串形式的特征码（如 "56 8B CF ? ? E8"）并预处理跳转表
byte_pattern &byte_pattern::set_pattern(const char *pattern_literal)
{
    this->_pattern.clear();
    this->_mask.clear();
    this->transform_pattern(pattern_literal);
    this->bm_preprocess();

    return *this;
}

// 设置二进制形式的特征码（全部字节精确匹配）
byte_pattern &byte_pattern::set_pattern(const void *pattern_binary, size_t size)
{
    this->_pattern.clear();
    this->_mask.clear();
    this->_pattern.assign(reinterpret_cast<const uint8_t *>(pattern_binary),
                          reinterpret_cast<const uint8_t *>(pattern_binary) + size);
    this->_mask.resize(size, 0xFF);
    this->bm_preprocess();

    return *this;
}

// 把搜索范围重置为当前模块（宿主 exe 或本 DLL）
byte_pattern &byte_pattern::reset_module()
{
    static HMODULE default_module = GetModuleHandleA(nullptr);

    return set_module(default_module);
}

// 把搜索范围设为指定模块的整个映像
byte_pattern &byte_pattern::set_module(memory_pointer module)
{
    this->get_module_range(module);

    return *this;
}

// 直接指定搜索的起止地址
byte_pattern &byte_pattern::set_range(memory_pointer beg, memory_pointer end)
{
    this->_range.first  = beg;
    this->_range.second = end;

    return *this;
}

// 按当前特征码执行搜索
byte_pattern &byte_pattern::search()
{
    this->bm_search();

    return *this;
}

// 设置字符串特征码并立即搜索
byte_pattern &byte_pattern::find_pattern(const char *pattern_literal)
{
    this->set_pattern(pattern_literal).search();

    return *this;
}

// 设置二进制特征码并立即搜索
byte_pattern &byte_pattern::find_pattern(const void *pattern_binary, size_t size)
{
    this->set_pattern(pattern_binary, size).search();

    return *this;
}

// 以空格为分隔，把特征码字符串拆成一个个子串（如 "56"、"?"、"E8"）
std::vector<std::string> byte_pattern::split_pattern(const char *literal)
{
    std::vector<std::string> result;
    std::string              sub_pattern;

    while (true)
    {
        // 遇到空格或字符串结束，就把当前子串收入结果
        if (*literal == ' ' || *literal == 0)
        {
            if (!sub_pattern.empty())
            {
                result.push_back(sub_pattern);
            }

            sub_pattern.clear();
        }
        else
        {
            sub_pattern += *literal;
        }

        if (*literal == 0)
        {
            break;
        }

        ++literal;
    }

    return result;
}

// 解析单个子串为一个字节值 + 掩码：
//   "5A" -> (0x5A, 0xFF)  精确匹配
//   "??" -> (0x00, 0x00)  任意字节
//   "?A" -> (0x0A, 0x0F)  低半字节匹配
//   "A?" -> (0xA0, 0xF0)  高半字节匹配
pair<uint8_t, uint8_t> byte_pattern::parse_sub_pattern(std::string_view sub)
{
    // 单个十六进制字符转数值
    auto digit_to_value = [](char character) {
        if ('0' <= character && character <= '9')
            return (character - '0');
        else if ('A' <= character && character <= 'F')
            return (character - 'A' + 10);
        else if ('a' <= character && character <= 'f')
            return (character - 'a' + 10);
        throw invalid_argument("Could not parse pattern.");
    };

    pair<uint8_t, uint8_t> result;

    if (sub.size() == 1)
    {
        if (sub[0] == '?')
        {
            result.first  = 0;
            result.second = 0;
        }
        else
        {
            result.first  = digit_to_value(sub[0]);
            result.second = 0xFF;
        }
    }
    else if (sub.size() == 2)
    {
        if (sub[0] == '?' && sub[1] == '?')
        {
            result.first  = 0;
            result.second = 0;
        }
        else if (sub[0] == '?')
        {
            result.first  = digit_to_value(sub[1]);
            result.second = 0xF;
        }
        else if (sub[1] == '?')
        {
            result.first  = (digit_to_value(sub[0]) << 4);
            result.second = 0xF0;
        }
        else
        {
            result.first  = ((digit_to_value(sub[0]) << 4) | digit_to_value(sub[1]));
            result.second = 0xFF;
        }
    }
    else
    {
        throw invalid_argument("Could not parse pattern.");
    }

    return result;
}

// 把字符串特征码整体转换为 _pattern（字节）与 _mask（掩码）两个数组
void byte_pattern::transform_pattern(const char *literal)
{
    vector<string> sub_patterns;

    if (literal == nullptr)
    {
        return;
    }

    sub_patterns = split_pattern(literal);

    for (auto sub : sub_patterns)
    {
        auto pat = parse_sub_pattern(sub);

        this->_pattern.push_back(pat.first);
        this->_mask.push_back(pat.second);
    }
}

// 解析 PE 头，取得模块映像的内存范围 [模块基址, 基址 + SizeOfImage)
void byte_pattern::get_module_range(memory_pointer module)
{
    // Range of whole image.
    PIMAGE_DOS_HEADER dosHeader = module.p<IMAGE_DOS_HEADER>();
    PIMAGE_NT_HEADERS ntHeader  = module.p<IMAGE_NT_HEADERS>(dosHeader->e_lfanew);

    _range.first  = module;
    _range.second = module.i(ntHeader->OptionalHeader.SizeOfImage);
}

// 清空特征码与搜索结果
void byte_pattern::clear()
{
    this->_pattern.clear();
    this->_mask.clear();
    this->_results.clear();
}

// 返回搜索结果个数
size_t byte_pattern::count() const
{
    return this->_results.size();
}

// 判断搜索结果个数是否正好等于 expected（常用于确认特征码唯一）
bool byte_pattern::has_size(size_t expected) const
{
    return (this->_results.size() == expected);
}

// 是否没有任何搜索结果
bool byte_pattern::empty() const
{
    return this->_results.empty();
}

// Boyer-Moore-Horspool 预处理：对每个可能的字节值，记录模式串中最后一次出现的位置
void byte_pattern::bm_preprocess()
{
    const uint8_t *pbytes      = this->_pattern.data();
    const uint8_t *pmask       = this->_mask.data();
    size_t         pattern_len = this->_pattern.size();

    for (uint32_t bc = 0; bc < 256; ++bc)
    {
        std::ptrdiff_t index;

        // 从后向前查找与该字节值匹配的位置
        for (index = pattern_len - 1; index >= 0; --index)
        {
            if ((pbytes[index] & pmask[index]) == (bc & pmask[index]))
            {
                break;
            }
        }

        this->_bmbc[bc] = index;
    }
}

// Boyer-Moore-Horspool 搜索：在 _range 范围内找出全部匹配位置
void byte_pattern::bm_search()
{
    const uint8_t *pbytes      = this->_pattern.data();
    const uint8_t *pmask       = this->_mask.data();
    size_t         pattern_len = this->_pattern.size();

    this->_results.clear();

    if (pattern_len == 0)
    {
        return;
    }

    uint8_t *range_begin = reinterpret_cast<uint8_t *>(this->_range.first);
    uint8_t *range_end   = reinterpret_cast<uint8_t *>(this->_range.second - pattern_len);

    ptrdiff_t index;

    // 访问游戏内存可能触发访问违例，这里用 SEH 兜底，遇到越界直接结束搜索
    __try
    {
        while (range_begin <= range_end)
        {
            // 从后向前逐字节比较（带掩码）
            for (index = pattern_len - 1; index >= 0; --index)
            {
                if ((pbytes[index] & pmask[index]) != (range_begin[index] & pmask[index]))
                {
                    break;
                }
            }

            if (index == -1)
            {
                // 完全匹配，记录结果并跳过整个模式长度继续搜索
                this->_results.emplace_back(range_begin);
                range_begin += pattern_len;
            }
            else
            {
                // 不匹配，按 BMH 跳转表向右移动
                range_begin += max(index - this->_bmbc[range_begin[index]], 1);
            }
        }
    }
    __except ((GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION) ? EXCEPTION_EXECUTE_HANDLER
                                                                 : EXCEPTION_CONTINUE_SEARCH)
    {
    }
}
