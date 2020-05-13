/*
 *  Path.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Path.hpp"


namespace bee {


/*
 ***************************************
 *
 * Internal helper utilities
 *
 ***************************************
 */
bool is_slash(const char* str)
{
    if (str == nullptr)
    {
        return false;
    }

    return *str == Path::generic_slash || *str == Path::preferred_slash;
}

i32 next_slash_pos(const StringView& string, const i32 start_index)
{
    if (start_index > string.size())
    {
        return -1;
    }

    int slash_idx = start_index;
    for (; slash_idx < string.size(); ++slash_idx)
    {
        if (is_slash(string.data() + slash_idx))
        {
            break;
        }
    }
    return slash_idx;
}


/*
 ***************************************
 *
 * Path - implementation
 *
 ***************************************
 */

#if BEE_COMPILER_MSVC == 0
constexpr char Path::generic_slash;
#endif


Path::Path(Allocator* allocator) noexcept
    : data_(allocator)
{}

Path::Path(const StringView& src, Allocator* allocator) noexcept
    : data_(src, allocator)
{}

Path::Path(const Path& other) noexcept
{
    data_ = other.data_;
}

Path::Path(Path&& other) noexcept
    : data_(std::move(other.data_))
{}

Path& Path::operator=(Path&& other) noexcept
{
    data_ = std::move(other.data_);
    return *this;
}

i32 Path::get_last_slash() const
{
    for (int c = data_.size() - 1; c >= 0; --c)
    {
        if (data_[c] == preferred_slash || data_[c] == generic_slash)
        {
            while (c > 0)
            {
                if (!is_slash(data_.c_str() + c - 1))
                {
                    break;
                }
                --c;
            }

            return c;
        }
    }

    return -1;
}

i32 Path::get_first_slash() const
{
    for (int c = 0; c < data_.size(); ++c)
    {
        if (data_[c] == preferred_slash || data_[c] == generic_slash)
        {
            return c;
        }
    }

    return -1;
}

Path Path::join(const StringView& src, Allocator* allocator) const
{
    return Path(data_.view(), allocator).append(src);
}

Path& Path::append(const StringView& src)
{
    if (src.empty())
    {
        return *this;
    }

    // Replace the current path with the entirety of str if it's absolute
    if (is_absolute(src))
    {
        data_ = src;
    }
    else
    {
        if (!data_.empty() && data_.back() != preferred_slash)
        {
            data_ += preferred_slash;
        }

        data_ += src;
    }

    return *this;
}

StringView Path::extension() const
{
    const auto last_dot = str::last_index_of(data_, '.');
    if (last_dot == -1)
    {
        return {};
    }

    return str::substring(data_, last_dot);
}

Path& Path::append_extension(const StringView& ext)
{
    if (ext.empty())
    {
        return *this;
    }

    if (data_.back() != '.')
    {
        data_ += ".";
    }

    if (ext.size() > 0 && ext[0] == '.')
    {
        data_.append(ext.data() + 1);
    }
    else
    {
        data_ += ext;
    }
    return *this;
}

Path& Path::set_extension(const StringView& ext)
{
    auto dotpos = str::last_index_of(data_, '.');
    const auto is_dot_slash = data_.size() > dotpos && (is_slash(data_.c_str() + dotpos + 1));
    const auto is_dot_dot = data_.size() > dotpos && data_[dotpos + 1] == '.';
    const auto empty_dot = ext.empty() || (ext[0] == '.' && ext.size() < 2);

    if (is_dot_dot || is_dot_slash)
    {
        dotpos = -1;
    }

    // If it's an empty extension we just want to end the string at the last dot position
    if (empty_dot)
    {
        if (dotpos != -1)
        {
            data_.resize(dotpos);
        }

        return *this;
    }

    const auto* ext_without_dot = ext.c_str();
    if (ext[0] == '.')
    {
        // don't use the dot that the user supplies - we'll append one manually later if it's missing
        ext_without_dot = ext.c_str() + 1;
    }

    // Couldn't find an extension so append a dot to begin one
    if (dotpos == -1)
    {
        data_.append('.');
        dotpos = data_.size() - 1;
    }

    // If the dot is the last character in the string we want to append rather than replace range
    if (dotpos + 1 < data_.size())
    {
        str::replace_range(&data_, dotpos + 1, data_.size() - (dotpos + 1), ext_without_dot);
    }
    else
    {
        data_.append(ext_without_dot);
    }

    return *this;
}

i32 Path::get_filename_idx() const
{
    int filename_idx = data_.size();

    // Skip slashes at the end of a path
    while (filename_idx > 0 && is_slash(data_.c_str() + filename_idx))
    {
        --filename_idx;
    }

    // Now get the actual filename begin index
    while (filename_idx > 0)
    {
        // Check one previous for slash so we get, i.e. '/Data/File.txt' -> 'File.txt' instead of '/File.txt'
        if (is_slash(data_.c_str() + filename_idx - 1))
        {
            break;
        }
        --filename_idx;
    }

    return filename_idx >= 0 ? filename_idx : 0;
}

StringView Path::filename() const
{
    return str::substring(data_, get_filename_idx());
}

Path& Path::remove_filename()
{
    if (!data_.empty())
    {
        data_.remove(get_filename_idx());
    }
    return *this;
}

Path& Path::replace_filename(const StringView& replacement)
{
    remove_filename();
    if (!replacement.empty())
    {
        append(replacement);
    }
    return *this;
}

StringView Path::stem() const
{
    const auto last_dot = str::last_index_of(data_, '.');
    if (last_dot == -1)
    {
        return data_.view();
    }

    const auto last_slash = get_last_slash() + 1; // ensures index is 0 if there is no slash
    return str::substring(data_, last_slash, last_dot - last_slash);
}

StringView Path::parent_view() const
{
    auto last_slash = get_last_slash();
    if (last_slash == -1)
    {
        return view();
    }

    return str::substring(data_, 0, last_slash);
}

Path Path::parent_path(Allocator* allocator) const
{
    return Path(parent_view(), allocator);
}

StringView Path::view() const
{
    return data_.view();
}

String Path::to_string(Allocator* allocator) const
{
    return String(data_.view(), allocator);
}

const char* Path::c_str() const
{
    return data_.c_str();
}

Path& Path::make_generic()
{
    for (auto& c : data_)
    {
        if (c == preferred_slash)
        {
            c = generic_slash;
        }
    }
    return *this;
}

Path Path::get_generic(Allocator* allocator) const
{
    Path generic_path(view(), allocator);
    generic_path.make_generic();
    return std::move(generic_path);
}

String Path::to_generic_string(Allocator* allocator) const
{
    String generic_str(data_.view(), allocator);
    for (auto& c : generic_str)
    {
        if (c == preferred_slash)
        {
            c = generic_slash;
        }
    }
    return generic_str;
}

String Path::preferred_string(Allocator* allocator) const
{
    String preferred_str(data_.view(), allocator);
    for (auto& c : preferred_str)
    {
        if (c == generic_slash)
        {
            c = preferred_slash;
        }
    }
    return preferred_str;
}

Path Path::relative_path(Allocator* allocator) const
{
    const auto first_slash = get_first_slash();
    if (first_slash < 0)
    {
        return *this;
    }

    return Path(str::substring(data_, first_slash + 1), allocator);
}

Path Path::relative_to(const Path& other, Allocator* allocator) const
{
    Path result(allocator);

    /* Examples:
     * "D:\Root" ("D:\Root\Another\Path") -> "..\..\Root"
     * "D:\Root\Another\Path" ("D:\Root") -> "Another\Path"
     * "D:\Root" ("C:\Root") -> "..\..\D:\Root"
     * "/a/d" ("/b/c") -> "../../a/d"
     */

    // in-place definition of std::mismatch to avoid including <algorithm>
    auto this_iter = begin();
    auto other_iter = other.begin();
    for (; this_iter != end() && other_iter != other.end() && *this_iter == *other_iter;)
    {
        ++this_iter;
        ++other_iter;
    }

    for (; other_iter != other.end(); ++other_iter)
    {
        result.append("..");
    }

    if (result.empty())
    {
        result.append(".");
    }

    for (; this_iter != end(); ++this_iter)
    {
        result.append(*this_iter);
    }

    return result;
}

bool Path::is_absolute() const
{
    return is_absolute(view());
}

i32 Path::size() const
{
    return sign_cast<i32>(data_.size());
}

PathIterator Path::begin() const
{
    return PathIterator(*this);
}

PathIterator Path::end() const
{
    return PathIterator(StringView(data_.c_str() + data_.size(), 0));
}


/*
 ***************************************
 *
 * Path free function
 *
 ***************************************
 */
// Internal path_compare function
i32 path_compare_impl(const StringView& lhs, const StringView& rhs)
{
    int lhs_index = 0;
    int rhs_index = 0;
    while (true)
    {
        const auto lhs_empty = lhs_index >= lhs.size();
        const auto rhs_empty = rhs_index >= rhs.size();
        if (lhs_empty || rhs_empty)
        {
            return static_cast<i32>(lhs_empty - rhs_empty);
        }

        const auto lhs_is_slash = is_slash(lhs.data() + lhs_index);
        const auto rhs_is_slash = is_slash(rhs.data() + rhs_index);

        if (lhs_is_slash != rhs_is_slash)
        {
            return next_slash_pos(lhs, lhs_index) - next_slash_pos(rhs, rhs_index);
        }

        // Skip all the slashes
        while (lhs_index < lhs.size() && is_slash(lhs.data() + lhs_index))
        {
            ++lhs_index;
        }

        while (rhs_index < rhs.size() && is_slash(rhs.data() + rhs_index))
        {
            ++rhs_index;
        }

        if (lhs[lhs_index] != rhs[rhs_index])
        {
            if (lhs_index == rhs_index)
            {
                return lhs[lhs_index] - rhs[rhs_index];
            }

            break;
        }

        ++lhs_index;
        ++rhs_index;
    }

    return lhs_index - rhs_index;
}

i32 path_compare(const Path& lhs, const Path& rhs)
{
    return path_compare_impl(lhs.view(), rhs.view());
}

i32 path_compare(const Path& lhs, const StringView& rhs)
{
    return path_compare_impl(lhs.view(), rhs);
}

i32 path_compare(const StringView& lhs, const Path& rhs)
{
    return path_compare_impl(lhs, rhs.view());
}

StringView path_get_extension(const char* c_string)
{
    return path_get_extension(StringView(c_string));
}

StringView path_get_extension(const StringView& path)
{
    const auto last_dot = str::last_index_of(path, '.');
    if (last_dot == -1)
    {
        return "";
    }

    return str::substring(path, last_dot);
}


/*
 ***************************************
 *
 * PathIterator - implementation
 *
 ***************************************
 */
PathIterator::PathIterator(const Path& path)
    : path_(path.c_str()),
      path_size_(path.size()),
      component_(path.c_str())
{
    next();
}

PathIterator::PathIterator(const StringView& path)
    : path_(path.c_str()),
      path_size_(path.size()),
      component_(path.c_str())
{
    next();
}

PathIterator::PathIterator(const PathIterator& other)
{
    copy_construct(other);
}

PathIterator& PathIterator::operator=(const bee::PathIterator& other)
{
    copy_construct(other);
    return *this;
}

StringView PathIterator::operator*() const
{
    if (path_ == nullptr || component_ - path_ > path_size_)
    {
        return {};
    }

    return { component_, component_size_ };
}

PathIterator& PathIterator::operator++()
{
    next();
    return *this;
}

const PathIterator PathIterator::operator++(int)
{
    const auto iter = *this;
    next();
    return iter;
}

bool PathIterator::operator==(const bee::PathIterator& other) const
{
    return component_ == other.component_ && component_size_ == other.component_size_;
}

void PathIterator::copy_construct(const bee::PathIterator& other)
{
    path_ = other.path_;
    path_size_ = other.path_size_;
    component_ = other.component_;
    component_size_ = other.component_size_;
}

void PathIterator::next()
{
    if (path_ == nullptr)
    {
        return;
    }

    component_ += component_size_;
    while (is_slash(component_))
    {
        ++component_;
    }

    const auto path_end = path_ + path_size_;
    const char* component_end = component_;

    while (component_end != path_end)
    {
#if BEE_OS_WINDOWS == 1
        const auto is_separator = is_slash(component_end) || (component_end > path_ && *(component_end - 1) == Path::colon);
#else
        const auto is_separator = is_slash(path_ + current);
#endif // BEE_OS_WINDOWS == 1

        if (is_separator)
        {
            break;
        }

        ++component_end;
    }

    component_size_ = sign_cast<i32>(component_end - component_);
}




} // namespace bee
