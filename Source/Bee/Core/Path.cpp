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

static i32 get_filename_index(const StringView& path)
{
    int filename_idx = path.size();

    // Skip slashes at the end of a path
    while (filename_idx > 0 && is_slash(path.data() + filename_idx))
    {
        --filename_idx;
    }

    // Now get the actual filename begin index
    while (filename_idx > 0)
    {
        // Check one previous for slash so we get, i.e. '/Data/File.txt' -> 'File.txt' instead of '/File.txt'
        if (is_slash(path.data() + filename_idx - 1))
        {
            break;
        }
        --filename_idx;
    }

    return filename_idx >= 0 ? filename_idx : 0;
}

static i32 get_last_slash(const StringView& path)
{
    for (int c = path.size() - 1; c >= 0; --c)
    {
        if (path[c] == Path::preferred_slash || path[c] == Path::generic_slash)
        {
            while (c > 0)
            {
                if (!is_slash(path.c_str() + c - 1))
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

static i32 get_first_slash(const StringView& path)
{
    for (int c = 0; c < path.size(); ++c)
    {
        if (path[c] == Path::preferred_slash || path[c] == Path::generic_slash)
        {
            return c;
        }
    }

    return -1;
}


/*
 ***************************************
 *
 * PathView - implementation
 *
 ***************************************
 */
PathView::PathView(const Path& path)
    : data_(path.c_str(), path.size())
{}

PathView::PathView(const StringView& path)
    : data_(path)
{}

PathView::PathView(const char* path)
    : data_(path)
{}

PathView& PathView::operator=(const char* other) noexcept
{
    data_ = other;
    return *this;
}

StringView PathView::string_view() const
{
    return data_;
}

StringView PathView::extension() const
{
    const auto last_dot = str::last_index_of(data_, '.');
    if (last_dot == -1)
    {
        return {};
    }

    return str::substring(data_, last_dot);
}

StringView PathView::filename() const
{
    return str::substring(data_, get_filename_index(data_));
}

bool PathView::has_root_directory() const
{
    return !root_directory().empty();
}

bool PathView::has_root_path() const
{
    return has_root_name() && has_root_directory();
}

PathView PathView::root_directory() const
{
    const auto root_name_size = root_name().size();

    if (root_name_size >= data_.size() || !is_slash(data_.begin() + root_name_size))
    {
        return StringView{};
    }

    int root_dir_size = 0;
    const auto* root_dir_begin = data_.begin() + root_name_size;

    while (is_slash(root_dir_begin + root_dir_size))
    {
        ++root_dir_size;
    }

    return StringView(root_dir_begin, root_dir_size);
}

PathView PathView::root_path() const
{
    const auto root_path_size = root_name().size() + root_directory().size();

    if (root_path_size <= 0)
    {
        return StringView{};
    }

    return StringView(data_.begin(), root_path_size);
}

PathView PathView::parent() const
{
    auto last_slash = get_last_slash(data_);
    if (last_slash == -1)
    {
        return data_;
    }

    return str::substring(data_, 0, last_slash);
}

StringView PathView::stem() const
{
    const auto last_dot = str::last_index_of(data_, '.');
    if (last_dot == -1)
    {
        return data_;
    }

    const auto last_slash = get_last_slash(data_) + 1; // ensures index is 0 if there is no slash
    return str::substring(data_, last_slash, last_dot - last_slash);
}

PathView PathView::relative_path() const
{
    const auto first_slash = get_first_slash(data_);

    if (first_slash < 0)
    {
        return *this;
    }

    return str::substring(data_, first_slash + 1);
}

bool PathView::is_relative_to(const PathView& other) const
{
    // in-place definition of std::mismatch to avoid including <algorithm>
    auto this_iter = begin();
    auto other_iter = other.begin();
    for (; this_iter != end() && other_iter != other.end() && *this_iter == *other_iter;)
    {
        ++this_iter;
        ++other_iter;
    }

    return other_iter == other.end() && this_iter != end();
}

PathIterator PathView::begin() const
{
    return PathIterator(*this);
}

PathIterator PathView::end() const
{
    return PathIterator(StringView(data_.c_str() + data_.size(), 0));
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

Path::Path(const PathView& src, Allocator* allocator) noexcept
    : data_(StringView(src.data(), src.size()), allocator)
{}

Path::Path(const Path& other) noexcept
{
    data_ = other.data_;
}

Path::Path(Path&& other) noexcept
    : data_(BEE_MOVE(other.data_))
{}

Path& Path::operator=(const PathView& path) noexcept
{
    if (path.data() >= data_.data() && path.data() <= data_.data() + data_.size())
    {
        memmove(data_.data(), path.data(), path.size());
        data_.resize(path.size());
    }
    else
    {
        data_.assign(path.string_view());
    }
    return *this;
}

Path& Path::operator=(const char* path) noexcept
{
    if (path >= data_.data() && path <= data_.data() + data_.size())
    {
        const int size = str::length(path);
        memmove(data_.data(), path, size);
        data_.resize(size);
    }
    else
    {
        data_.assign(path);
    }
    return *this;
}

Path& Path::operator=(Path&& other) noexcept
{
    data_ = BEE_MOVE(other.data_);
    return *this;
}

Path Path::join(const PathView& src, Allocator* allocator) const
{
    return Path(data_.view(), allocator).append(src);
}

Path& Path::append(const PathView& src)
{
    if (src.empty())
    {
        return *this;
    }

    // Replace the current path with the entirety of str if it's absolute
    if (src.is_absolute())
    {
        data_ = src.string_view();
    }
    else
    {
        if (!data_.empty() && data_.back() != preferred_slash)
        {
            data_ += preferred_slash;
        }

        data_ += src.string_view();
    }

    return *this;
}

Path& Path::prepend(const PathView& src)
{
    if (src.empty())
    {
        return *this;
    }

    if (data_.size() > 1 && data_[0] == '.' && is_slash(data_.begin() + 1))
    {
        data_.remove(0, 1); // paths with the form ./Path but not ../Path
    }

    // Overwrite any slashes in the current path with the last slashes in src
    if (is_slash(src.data() + src.size() - 1))
    {
        str::trim_start(&data_, preferred_slash);
        str::trim_start(&data_, generic_slash);
    }

    // Always prepend the src even if its absolute
    data_.insert(0, src.string_view());
    return *this;
}

StringView Path::extension() const
{
    return view().extension();
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

    if (!ext.empty() && ext[0] == '.')
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

bool Path::exists() const
{
    return view().exists();
}

StringView Path::filename() const
{
    return view().filename();
}

Path& Path::remove_filename()
{
    if (!data_.empty())
    {
        data_.remove(get_filename_index(data_.view()));
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

bool Path::has_root_name() const
{
    return view().has_root_name();
}

bool Path::has_root_directory() const
{
    return view().has_root_directory();
}

bool Path::has_root_path() const
{
    return view().has_root_path();
}

PathView Path::root_name() const
{
    return view().root_name();
}

PathView Path::root_directory() const
{
    return view().root_directory();
}

PathView Path::root_path() const
{
    return view().root_path();
}

StringView Path::stem() const
{
    return view().stem();
}

PathView Path::parent() const
{
    return view().parent();
}

PathView Path::view() const
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
    return BEE_MOVE(generic_path);
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

Path& Path::make_preferred()
{
    for (auto& c : data_)
    {
        if (c == generic_slash)
        {
            c = preferred_slash;
        }
    }
    return *this;
}

PathView Path::relative_path() const
{
    return view().relative_path();
}

Path Path::relative_to(const PathView& other, Allocator* allocator) const
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

    for (; this_iter != end(); ++this_iter)
    {
        result.append(*this_iter);
    }

    return BEE_MOVE(result);
}

bool Path::is_relative_to(const PathView& other) const
{
    return view().is_relative_to(other);
}

bool Path::is_absolute() const
{
    return view().is_absolute();
}

i32 Path::size() const
{
    return sign_cast<i32>(data_.size());
}

PathIterator Path::begin() const
{
    return view().begin();
}

PathIterator Path::end() const
{
    return view().end();
}


/*
 ***************************************
 *
 * Path free function
 *
 ***************************************
 */
static i32 skip_slash(const PathView& path, const i32 offset)
{
    for (int i = offset; i < path.size(); ++i)
    {
        if (!is_slash(path.data() + i))
        {
            return i;
        }
    }

    return path.size();
}

i32 path_compare(const PathView& lhs, const PathView& rhs)
{
    if (lhs.empty() || rhs.empty())
    {
        return lhs.size() - rhs.size();
    }

    int lhs_index = 0;
    int rhs_index = 0;
    int lhs_components = 1;
    int rhs_components = 1;

    while (lhs_index < lhs.size() && rhs_index < rhs.size())
    {
        if (lhs.data()[lhs_index] != rhs.data()[rhs_index])
        {
            return lhs.data()[lhs_index] - rhs.data()[rhs_index];
        }

        const int lhs_prev_index = lhs_index;
        const int rhs_prev_index = rhs_index;
        lhs_index = skip_slash(lhs, lhs_index + 1);
        rhs_index = skip_slash(rhs, rhs_index + 1);

        if (lhs_index > lhs_prev_index + 1)
        {
            ++lhs_components;
        }
        if (rhs_index > rhs_prev_index + 1)
        {
            ++rhs_components;
        }
    }

    if (lhs_components != rhs_components)
    {
        return lhs_components - rhs_components;
    }

    return (lhs_index - lhs.size()) - (rhs_index - rhs.size());


//    const auto lhs_sv = lhs.string_view();
//    const auto rhs_sv = rhs.string_view();
//
//    int lhs_index = 0;
//    int rhs_index = 0;
//    while (true)
//    {
//        const auto lhs_empty = lhs_index >= lhs.size();
//        const auto rhs_empty = rhs_index >= rhs.size();
//        if (lhs_empty || rhs_empty)
//        {
//            return static_cast<i32>(lhs_empty - rhs_empty);
//        }
//
//        const auto lhs_is_slash = is_slash(lhs.data() + lhs_index);
//        const auto rhs_is_slash = is_slash(rhs.data() + rhs_index);
//
//        if (lhs_is_slash != rhs_is_slash)
//        {
//            return next_slash_pos(lhs_sv, lhs_index) - next_slash_pos(rhs_sv, rhs_index);
//        }
//
//        // Skip all the slashes
//        while (lhs_index < lhs.size() && is_slash(lhs.data() + lhs_index))
//        {
//            ++lhs_index;
//        }
//
//        while (rhs_index < rhs.size() && is_slash(rhs.data() + rhs_index))
//        {
//            ++rhs_index;
//        }
//
//        if (lhs_sv[lhs_index] != rhs_sv[rhs_index])
//        {
//            if (lhs_index == rhs_index)
//            {
//                return lhs_sv[lhs_index] - rhs_sv[rhs_index];
//            }
//
//            break;
//        }
//
//        ++lhs_index;
//        ++rhs_index;
//    }
//
//    return lhs_index - rhs_index;
}


/*
 ***************************************
 *
 * PathIterator - implementation
 *
 ***************************************
 */
PathIterator::PathIterator(const PathView& path)
    : path_(path.data()),
      path_size_(path.size()),
      component_(path.data())
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

PathView PathIterator::operator*() const
{
    if (path_ == nullptr || component_ - path_ > path_size_)
    {
        return {};
    }

    return StringView(component_, component_size_);
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
