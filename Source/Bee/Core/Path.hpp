/*
 *  Path.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/String.hpp"
#include "Bee/Core/Hash.hpp"


namespace bee {


class PathIterator;
class Path;
class SerializationBuilder;


class BEE_CORE_API PathView
{
public:
    constexpr PathView() noexcept = default;

    explicit PathView(const Path& path);

    PathView(const StringView& path);

    PathView(const char* path);

    PathView& operator=(const PathView& other) noexcept = default;

    PathView& operator=(const char* other) noexcept;

    StringView string_view() const;

    StringView extension() const;

    /// @brief Checks whether or not this path exists within the filesystem
    /// @return
    bool exists() const;

    /// @brief Gets the filename component of the path, i.e. given `/usr/local/bin/ls` this
    /// function would return `ls`
    /// @return
    StringView filename() const;

    /// @brief Returns true if the current path has a root name, i.e. given `C:\\Files\\file.txt`
    /// this function looks for`C:`
    bool has_root_name() const;

    /// @brief Returns true if the current path has a root directory, i.e. given `C:\\Files\\file.txt`
    /// this function looks for the first`\\`
    bool has_root_directory() const;

    /// @brief Returns true if the current path has a root path, i.e. given `C:\\Files\\file.txt`
    /// this function looks for`C:\\`
    bool has_root_path() const;

    /// @brief Returns the root name if this path object has one, otherwise returns an empty string
    PathView root_name() const;

    /// @brief Returns the root directory if this path object has one, otherwise returns an empty string
    PathView root_directory() const;

    /// @brief Returns the root path if this path object has one, otherwise returns an empty string
    PathView root_path() const;

    /// @brief Gets the absolute parent directory of the filename component, i.e. given
    /// `/usr/local/bin/ls` this function would return `bin`
    /// @return
    PathView parent() const;

    /// @brief Gets the filename component of the path without the extension, i.e. given `File.txt`
    /// this function would return `File`
    /// @return
    StringView stem() const;

    /// Gets the path relative to the root, i.e. `D:\Some\Path` would be `Some\Path`
    PathView relative_path() const;

    bool is_relative_to(const PathView& other) const;

    bool is_absolute() const;

    PathIterator begin() const;

    PathIterator end() const;

    inline const char* data() const
    {
        return data_.data();
    }

    inline i32 size() const
    {
        return data_.size();
    }

    inline bool empty() const
    {
        return data_.empty();
    }
private:
    StringView data_;
};


/// @brief The path class is a lightweight path utility class for navigating the platforms
/// filesystem
class BEE_REFLECT(serializable, use_builder) BEE_CORE_API Path
{
public:
#if BEE_OS_WINDOWS == 1
    static constexpr char   preferred_slash = '\\';
#else
    static constexpr char   preferred_slash = '/';
#endif // BEE_OS_WINDOWS == 1

    static constexpr char   generic_slash = '/';
    static constexpr char   colon = ':';

    Path(Allocator* allocator = system_allocator()) noexcept; // NOLINT(google-explicit-constructor,hicpp-explicit-conversions)

    Path(const PathView& src, Allocator* allocator = system_allocator()) noexcept; // NOLINT(google-explicit-constructor,hicpp-explicit-conversions)

    Path(const char* src, Allocator* allocator = system_allocator()) noexcept // NOLINT(google-explicit-constructor,hicpp-explicit-conversions)
        : Path(PathView(src), allocator)
    {}

    Path(const Path& other) noexcept;

    Path(Path&& other) noexcept;

    inline Path& operator=(const PathView& path) noexcept;

    inline Path& operator=(const char* path) noexcept;

    Path& operator=(const Path& other) = default;

    Path& operator=(Path&& other) noexcept;

    /// @brief Joins this path and another together and returns the result as a new Path object
    /// @param other
    Path join(const PathView& src, Allocator* allocator = system_allocator()) const;

    Path& append(const PathView& src);

    Path& prepend(const PathView& src);

    StringView extension() const;

    Path& append_extension(const StringView& ext);

    Path& set_extension(const StringView& ext);

    /// @brief Checks whether or not this path exists within the filesystem
    /// @return
    bool exists() const;

    /// @brief Gets the filename component of the path, i.e. given `/usr/local/bin/ls` this
    /// function would return `ls`
    /// @return
    StringView filename() const;

    Path& remove_filename();

    Path& replace_filename(const StringView& replacement);

    /// @brief Returns true if the current path has a root name, i.e. given `C:\\Files\\file.txt`
    /// this function looks for`C:`
    bool has_root_name() const;

    /// @brief Returns true if the current path has a root directory, i.e. given `C:\\Files\\file.txt`
    /// this function looks for the first`\\`
    bool has_root_directory() const;

    /// @brief Returns true if the current path has a root path, i.e. given `C:\\Files\\file.txt`
    /// this function looks for`C:\\`
    bool has_root_path() const;

    /// @brief Returns the root name if this path object has one, otherwise returns an empty string
    PathView root_name() const;

    /// @brief Returns the root directory if this path object has one, otherwise returns an empty string
    PathView root_directory() const;

    /// @brief Returns the root path if this path object has one, otherwise returns an empty string
    PathView root_path() const;

    /// @brief Gets the absolute parent directory of the filename component, i.e. given
    /// `/usr/local/bin/ls` this function would return `bin`
    /// @return
    PathView parent() const;

    /// @brief Gets the filename component of the path without the extension, i.e. given `File.txt`
    /// this function would return `File`
    /// @return
    StringView stem() const;

    /// Gets the path relative to the root, i.e. `D:\Some\Path` would be `Some\Path`
    PathView relative_path() const;

    /**
     * Returns a new path object relative to another path, i.e. given this path: `D:\Root`
     * and another path: `D:\Root\Another\Path`` the result would be `..\..\Root`
     */
    Path relative_to(const PathView& other, Allocator* allocator = system_allocator()) const;

    bool is_relative_to(const PathView& other) const;

    bool is_absolute() const;

    /// @brief Gets the raw string representation of this path
    /// @return
    PathView view() const;

    String to_string(Allocator* allocator = system_allocator()) const;

    const char* c_str() const;

    String to_generic_string(Allocator* allocator = system_allocator()) const;

    String preferred_string(Allocator* allocator = system_allocator()) const;

    Path& make_preferred();

    Path& make_generic();

    Path get_generic(Allocator* allocator = system_allocator()) const;

    /**
     * Converts the path to its absolute, normalized representation - all slashes are converted to the platforms
     * `preferred_slash` and symlinks removed
     */
    Path& normalize();

    Path get_normalized(Allocator* allocator = system_allocator()) const;

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

    inline Allocator* allocator()
    {
        return data_.allocator();
    }

private:
    friend void custom_serialize_type(SerializationBuilder* builder, Path* path);

    String data_;
};

/// @brief Gets the absolute path to the application binary's directory
/// @return
BEE_CORE_API PathView executable_path();

/// Gets a path to the current working directory
BEE_CORE_API PathView current_working_directory();

/// Compares two paths lexicographically and returns -1 if lhs < rhs, 0 if lhs == rhs, or 1 if lhs > rhs
BEE_CORE_API i32 path_compare(const PathView& lhs, const PathView& rhs);

/*
 ******************
 * #  Path/PathView
 * operator==
 ******************
 */
inline bool operator==(const PathView& lhs, const PathView& rhs)
{
    return path_compare(lhs, rhs) == 0;
}

inline bool operator==(const Path& lhs, const Path& rhs)
{
    return path_compare(lhs.view(), rhs.view()) == 0;
}

inline bool operator==(const Path& lhs, const PathView& rhs)
{
    return path_compare(lhs.view(), rhs) == 0;
}

inline bool operator==(const PathView& lhs, const Path& rhs)
{
    return path_compare(lhs, rhs.view()) == 0;
}

inline bool operator==(const PathView& lhs, const StringView& rhs)
{
    return path_compare(lhs, rhs) == 0;
}

inline bool operator==(const StringView& lhs, const PathView& rhs)
{
    return path_compare(lhs, rhs) == 0;
}

inline bool operator==(const PathView& lhs, const char* rhs)
{
    return path_compare(lhs, rhs) == 0;
}

inline bool operator==(const char* lhs, const PathView& rhs)
{
    return path_compare(lhs, rhs) == 0;
}

/*
 ******************
 * #  Path/PathView
 * operator!=
 ******************
 */
inline bool operator!=(const PathView& lhs, const PathView& rhs)
{
    return !(lhs == rhs);
}

inline bool operator!=(const Path& lhs, const Path& rhs)
{
    return !(lhs == rhs);
}

inline bool operator!=(const Path& lhs, const PathView& rhs)
{
    return !(lhs == rhs);
}

inline bool operator!=(const PathView& lhs, const Path& rhs)
{
    return !(lhs == rhs);
}

inline bool operator!=(const PathView& lhs, const StringView& rhs)
{
    return !(lhs == rhs);
}

inline bool operator!=(const StringView& lhs, const PathView& rhs)
{
    return !(lhs == rhs);
}

inline bool operator!=(const PathView& lhs, const char* rhs)
{
    return !(lhs == rhs);
}

inline bool operator!=(const char* lhs, const PathView& rhs)
{
    return !(lhs == rhs);
}

/*
 ******************
 * #  Path/PathView
 * operator<
 ******************
 */
inline bool operator<(const PathView& lhs, const PathView& rhs)
{
    return path_compare(lhs, rhs) < 0;
}

inline bool operator<(const Path& lhs, const Path& rhs)
{
    return path_compare(lhs.view(), rhs.view()) < 0;
}

inline bool operator<(const Path& lhs, const PathView& rhs)
{
    return path_compare(lhs.view(), rhs) < 0;
}

inline bool operator<(const PathView& lhs, const Path& rhs)
{
    return path_compare(lhs, rhs.view()) < 0;
}

inline bool operator<(const PathView& lhs, const StringView& rhs)
{
    return path_compare(lhs, rhs) < 0;
}

inline bool operator<(const StringView& lhs, const PathView& rhs)
{
    return path_compare(lhs, rhs) < 0;
}

inline bool operator<(const PathView& lhs, const char* rhs)
{
    return path_compare(lhs, rhs) < 0;
}

inline bool operator<(const char* lhs, const PathView& rhs)
{
    return path_compare(lhs, rhs) < 0;
}

/*
 ******************
 * #  Path/PathView
 * operator>
 ******************
 */
inline bool operator>(const PathView& lhs, const PathView& rhs)
{
    return path_compare(lhs, rhs) > 0;
}

inline bool operator>(const Path& lhs, const Path& rhs)
{
    return path_compare(lhs.view(), rhs.view()) > 0;
}

inline bool operator>(const Path& lhs, const PathView& rhs)
{
    return path_compare(lhs.view(), rhs) > 0;
}

inline bool operator>(const PathView& lhs, const Path& rhs)
{
    return path_compare(lhs, rhs.view()) > 0;
}

inline bool operator>(const PathView& lhs, const StringView& rhs)
{
    return path_compare(lhs, rhs) > 0;
}

inline bool operator>(const StringView& lhs, const PathView& rhs)
{
    return path_compare(lhs, rhs) > 0;
}

inline bool operator>(const PathView& lhs, const char* rhs)
{
    return path_compare(lhs, rhs) > 0;
}

inline bool operator>(const char* lhs, const PathView& rhs)
{
    return path_compare(lhs, rhs) > 0;
}

template <>
struct Hash<PathView>
{
    inline u32 operator()(const PathView& key) const
    {
        return get_hash(key.data(), key.size(), 0xF00D);
    }

    inline u32 operator()(const String& key) const
    {
        return get_hash(key);
    }

    inline u32 operator()(const StringView& key) const
    {
        return get_hash(key);
    }

    inline u32 operator()(const char* key) const
    {
        return get_hash(key, str::length(key), 0xF00D);
    }
};


template <>
struct Hash<Path>
{
    inline u32 operator()(const Path& key) const
    {
        return get_hash(key.view());
    }

    inline u32 operator()(const String& key) const
    {
        return get_hash(key);
    }

    inline u32 operator()(const StringView& key) const
    {
        return get_hash(key);
    }

    inline u32 operator()(const char* key) const
    {
        return get_hash(key, str::length(key), 0xF00D);
    }
};

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

    explicit PathIterator(const PathView& path);

    PathIterator(const PathIterator& other);

    PathIterator& operator=(const PathIterator& other);

    PathView operator*() const;

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


/*
 ************************************************
 *
 * Adapters for std hash-based containers
 *
 ************************************************
 */
namespace std {


template <>
struct hash<bee::PathView>
{
    inline size_t operator()(const bee::PathView& key) const
    {
        return bee::Hash<bee::PathView>{}(key);
    }

    inline size_t operator()(const bee::String& key) const
    {
        return bee::Hash<bee::PathView>{}(key);
    }

    inline size_t operator()(const bee::StringView& key) const
    {
        return bee::Hash<bee::PathView>{}(key);
    }

    inline size_t operator()(const char* key) const
    {
        return bee::Hash<bee::PathView>{}(key);
    }
};

template <>
struct hash<bee::Path>
{
    inline size_t operator()(const bee::Path& key) const
    {
        return bee::Hash<bee::Path>{}(key);
    }

    inline size_t operator()(const bee::String& key) const
    {
        return bee::Hash<bee::Path>{}(key);
    }

    inline size_t operator()(const bee::StringView& key) const
    {
        return bee::Hash<bee::Path>{}(key);
    }

    inline size_t operator()(const char* key) const
    {
        return bee::Hash<bee::Path>{}(key);
    }
};


}



