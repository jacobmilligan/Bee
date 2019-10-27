/*
 *  Path.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/String.hpp"


namespace bee {


class PathIterator;

class Path;


/// @brief The path class is a lightweight path utility class for navigating the platforms
/// filesystem
class BEE_CORE_API Path
{
public:
#if BEE_OS_WINDOWS == 1
    static constexpr char   preferred_slash = '\\';
#else
    static constexpr char   preferred_slash = '/';
#endif // BEE_OS_WINDOWS == 1

    static constexpr char   generic_slash_ = '/';
    static constexpr char   colon = ':';

    Path(Allocator* allocator = system_allocator()); // NOLINT(google-explicit-constructor,hicpp-explicit-conversions)

    Path(const StringView& src, Allocator* allocator = system_allocator()); // NOLINT(google-explicit-constructor,hicpp-explicit-conversions)

    Path(const Path& other);

    inline Path& operator=(const StringView& path)
    {
        data_ = path;
        return *this;
    }

    inline Path& operator=(const char* path)
    {
        data_ = path;
        return *this;
    }

    Path& operator=(const Path& other);

    /// @brief Gets the absolute path to the application binary's directory
    /// @return
    static Path executable_path(Allocator* allocator = system_allocator());

    /// Gets a path to the current working directory
    static Path current_working_directory(Allocator* allocator = system_allocator());

    /// @brief Joins this path and another together and returns the result as a new Path object
    /// @param other
    Path join(const StringView& src, Allocator* allocator = system_allocator()) const;

    inline Path join(const Path& src, Allocator* allocator = system_allocator()) const
    {
        return join(src.view(), allocator);
    }

    Path& append(const StringView& src);

    inline Path& append(const Path& src)
    {
        return append(src.view());
    }

    StringView extension() const;

    Path& append_extension(const StringView& ext);

    inline Path& append_extension(const Path& ext)
    {
        return append_extension(ext.view());
    }

    Path& set_extension(const StringView& ext);

    inline Path& set_extension(const Path& ext)
    {
        return set_extension(ext.view());
    }

    /// @brief Checks whether or not this path exists within the filesystem
    /// @return
    bool exists() const;

    /// @brief Gets the filename component of the path, i.e. given `/usr/local/bin/ls` this
    /// function would return `ls`
    /// @return
    StringView filename() const;

    Path& remove_filename();

    Path& replace_filename(const Path& replacement);

    Path& replace_filename(const StringView& replacement);

    /// @brief Gets the absolute parent directory of the filename component, i.e. given
    /// `/usr/local/bin/ls` this function would return `bin`
    /// @return
    Path parent(Allocator* allocator = system_allocator()) const;

    /// @brief Gets the filename component of the path without the extension, i.e. given `File.txt`
    /// this function would return `File`
    /// @return
    StringView stem() const;

    /// Gets the path relative to the root, i.e. `D:\Some\Path` would be `Some\Path`
    Path relative_path(Allocator* allocator = system_allocator()) const;

    bool is_absolute() const;

    /// @brief Gets the raw string representation of this path
    /// @return
    StringView view() const;

    String to_string(Allocator* allocator = system_allocator()) const;

    const char* c_str() const;

    String to_generic_string(Allocator* allocator = system_allocator()) const;

    String preferred_string(Allocator* allocator = system_allocator()) const;

    /**
     * Converts the path to its absolute, normalized representation - all slashes are converted to the platforms
     * `preferred_slash` and symlinks removed
     */
    Path& normalize();

    /// @brief Gets the character size of the path string
    /// @return
    i32 size() const;

    PathIterator begin() const;

    PathIterator end() const;

    inline bool empty() const
    {
        return size() <= 0;
    }

    inline void clear()
    {
        data_.clear();
    }
private:
    String data_;

    bool is_absolute(const StringView& path) const;

    i32 get_last_slash() const;

    i32 get_first_slash() const;

    i32 get_filename_idx() const;
};


/// Compares two paths lexicographically and returns -1 if lhs < rhs, 0 if lhs == rhs, or 1 if lhs > rhs
BEE_CORE_API i32 path_compare(const Path& lhs, const Path& rhs);

BEE_CORE_API i32 path_compare(const Path& lhs, const StringView& rhs);

BEE_CORE_API i32 path_compare(const StringView& lhs, const Path& rhs);

BEE_CORE_API StringView path_get_extension(const char* c_string);


inline bool operator==(const Path& lhs, const Path& rhs)
{
    return path_compare(lhs, rhs) == 0;
}

inline bool operator==(const Path& lhs, const StringView& rhs)
{
    return path_compare(lhs, rhs) == 0;
}

inline bool operator==(const StringView& lhs, const Path& rhs)
{
    return path_compare(lhs, rhs) == 0;
}

inline bool operator!=(const Path& lhs, const Path& rhs)
{
    return !(lhs == rhs);
}

inline bool operator!=(const Path& lhs, const StringView& rhs)
{
    return !(lhs == rhs);
}

inline bool operator!=(const StringView& lhs, const Path& rhs)
{
    return !(lhs == rhs);
}

/*
 ***************************************
 *
 * # PathIterator
 *
 * iterates over a paths components
 *
 ***************************************
 */
class BEE_CORE_API PathIterator
{
public:
    PathIterator() = default;

    explicit PathIterator(const Path& path);

    explicit PathIterator(const StringView& path);

    PathIterator(const PathIterator& other);

    PathIterator& operator=(const PathIterator& other);

    StringView operator*() const;

    const PathIterator operator++(int);

    PathIterator& operator++();

    bool operator==(const PathIterator& other) const;

    inline bool operator!=(const PathIterator& other) const
    {
        return !(*this == other);
    }
private:
    const char* path_ { nullptr };
    i32         path_size_ { 0 };
    const char* component_ { nullptr };
    i32         component_size_ { 0 };

    void copy_construct(const PathIterator& other);

    void next();
};


} // namespace bee


