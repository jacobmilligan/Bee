/*
 *  Reflection.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Enum.hpp"
#include "Bee/Core/Logger.hpp"
#include "Bee/Core/IO.hpp"
#include "Bee/Core/Handle.hpp"

#include <inttypes.h>


namespace bee {


/*
 * # Reflection macros
 *
 * These are also defined in Config.hpp so that reflection can be annotated without having to
 * #include Reflection.hpp and all of its dependencies
 */
#ifndef BEE_REFLECT
    #ifdef BEE_COMPILE_REFLECTION
        #define BEE_REFLECT(...) __attribute__((annotate("bee-reflect[" #__VA_ARGS__ "]")))
        #define BEE_DEPRECATED(decl, ...) BEE_REFLECT(deprecated, __VA_ARGS__) decl
    #else
        #define BEE_REFLECT(...)
        #define BEE_DEPRECATED(decl, ...)
    #endif // BEE_COMPILE_REFLECTION

    #define BEE_NONMEMBER(x) ::bee::ComplexTypeTag<::bee::get_static_string_hash(#x)>
    #define BEE_TEMPLATED(x) ::bee::ComplexTypeTag<::bee::get_static_string_hash(#x)>
#endif // BEE_REFLECT

/*
 * Whenever a new type is added here, BEE_SERIALIZER_INTERFACE must also be updated to match this macro
 */
#define BEE_BUILTIN_TYPES                                       \
    BEE_BUILTIN_TYPE(bool, bool)                                \
    BEE_BUILTIN_TYPE(char, char)                                \
    BEE_BUILTIN_TYPE(signed char, signed_char)                  \
    BEE_BUILTIN_TYPE(unsigned char, unsigned_char)              \
    BEE_BUILTIN_TYPE(short, short)                              \
    BEE_BUILTIN_TYPE(unsigned short, unsigned_short)            \
    BEE_BUILTIN_TYPE(int, int)                                  \
    BEE_BUILTIN_TYPE(unsigned int, unsigned_int)                \
    BEE_BUILTIN_TYPE(long, long)                                \
    BEE_BUILTIN_TYPE(unsigned long, unsigned_long)              \
    BEE_BUILTIN_TYPE(long long, long_long)                      \
    BEE_BUILTIN_TYPE(unsigned long long, unsigned_long_long)    \
    BEE_BUILTIN_TYPE(float, float)                              \
    BEE_BUILTIN_TYPE(double, double)                            \
    BEE_BUILTIN_TYPE(bee::u128, u128)                           \
    BEE_BUILTIN_TYPE(void, void)


BEE_FLAGS(Qualifier, u32)
{
    none        = 0u,
    cv_const    = 1u << 0u,
    cv_volatile = 1u << 1u,
    lvalue_ref  = 1u << 2u,
    rvalue_ref  = 1u << 3u,
    pointer     = 1u << 4u
};

BEE_FLAGS(StorageClass, u32)
{
    none                    = 0u,
    auto_storage            = 1u << 0u,
    register_storage        = 1u << 1u,
    static_storage          = 1u << 2u,
    extern_storage          = 1u << 3u,
    thread_local_storage    = 1u << 4u,
    mutable_storage         = 1u << 5u
};

BEE_FLAGS(TypeKind, u32)
{
    unknown                 = 0u,
    class_decl              = 1u << 0u,
    struct_decl             = 1u << 1u,
    enum_decl               = 1u << 2u,
    union_decl              = 1u << 3u,
    template_decl           = 1u << 4u,
    field                   = 1u << 5u,
    function                = 1u << 6u,
    method                  = 1u << 7u,
    fundamental             = 1u << 8u,
    array                   = 1u << 9u,
    record                  = class_decl | struct_decl | union_decl
};

enum class AttributeKind
{
    boolean,
    integer,
    floating_point,
    string,
    type,
    invalid
};

BEE_FLAGS(SerializationFlags, u32)
{
    none            = 0u,
    /**
     * This is the most memory and CPU efficient format for serialized data because it essentially maps to the current
     * memory layout of the struct without encoding extra information. However, this means that if a struct or classes fields
     * are reordered, renamed, or their types are changed in this format old data becomes unreadable meaning that
     * types serialized in packed_format are not version tolerant or backwards compatible in any way.
     */
    packed_format   = 1u << 0u,

    /**
     * In this format each field is stored as a key-value pair. The key contains a hash of the fields name and it's
     * accompanying type hash. Types serialized in this format are totally version tolerant and backwards and forwards
     * compatible - field keys are used as a lookup into the parent reflected type to find the latest offset for
     * that unique combination of type and field name and if the field is missing, it's just skipped.
     */
    table_format    = 1u << 1u,

    /**
     * This indicates that the type uses a SerializationBuilder in a custom serialization function to handle
     * adding/removing fields and implementing more complex behavior than the default serialization method allows
     */
    uses_builder    = 1u << 2u
};


#define BEE_BUILTIN_TYPE(type, name) name##_kind,
enum class FundamentalKind
{
    BEE_BUILTIN_TYPES
    count
};
#undef BEE_BUILTIN_TYPE


struct TypeInfo;

class BEE_CORE_API TypeInfo
{
public:
    TypeInfo();

    explicit constexpr TypeInfo(const TypeInfo* type)
        : type_(type)
    {}

    inline constexpr const TypeInfo* operator->() const
    {
        BEE_ASSERT(type_ != nullptr);
        return type_;
    }

    inline constexpr const TypeInfo& operator*() const
    {
        BEE_ASSERT(type_ != nullptr);
        return *type_;
    }

    inline constexpr const TypeInfo* get() const
    {
        return type_;
    }

    inline bool is_unknown() const;

private:
    const TypeInfo* type_ {nullptr };
};

template <typename T>
class SpecializedType
{
public:
    static_assert(std::is_base_of_v<TypeInfo, T>, "`T` must derive from bee::BasicType");

    ~SpecializedType()
    {
        type_ = nullptr;
    }

    explicit constexpr SpecializedType(const T* type)
        : type_(type)
    {}

    inline constexpr const T* operator->() const
    {
        BEE_ASSERT(type_ != nullptr);
        return type_;
    }

    inline constexpr const T& operator*() const
    {
        BEE_ASSERT(type_ != nullptr);
        return *type_;
    }

    inline constexpr const T* get() const
    {
        return type_;
    }

    BEE_CORE_API inline bool is_unknown() const;

private:
    const T* type_ { nullptr };
};

class BEE_CORE_API namespace_iterator
{
public:
    using value_type    = StringView;
    using reference     = StringView&;
    using pointer       = StringView*;

    explicit namespace_iterator(const Type& type);

    explicit namespace_iterator(const StringView& fully_qualified_name);

    namespace_iterator(const namespace_iterator& other) = default;

    namespace_iterator& operator=(const namespace_iterator& other) = default;

    StringView operator*() const;

    StringView operator->() const;

    namespace_iterator& operator++();

    bool operator==(const namespace_iterator& other) const;

    inline bool operator!=(const namespace_iterator& other) const
    {
        return !(*this == other);
    }
private:
    const char* current_ { nullptr };
    const char* end_ { nullptr };
    i32         size_ { 0 };

    void next_namespace();

    StringView view() const;
};


struct BEE_CORE_API NamespaceRangeAdapter
{
    Type type { nullptr };

    namespace_iterator begin() const;

    namespace_iterator end() const;
};


struct NamespaceRangeFromNameAdapter
{
    const StringView& fully_qualified_name { nullptr };

    namespace_iterator begin() const
    {
        return namespace_iterator(fully_qualified_name);
    }

    namespace_iterator end() const
    {
        return namespace_iterator(StringView(fully_qualified_name.data() + fully_qualified_name.size(), 0));
    }
};


inline NamespaceRangeFromNameAdapter get_namespaces_from_name(const StringView& fully_qualified_type_name)
{
    return NamespaceRangeFromNameAdapter { fully_qualified_type_name };
}

inline StringView get_unqualified_name(const StringView& fully_qualified_name)
{
    const auto last_ns = str::last_index_of(fully_qualified_name, "::");
    return last_ns >= 0 ? str::substring(fully_qualified_name, last_ns + 2) : fully_qualified_name;
}


template <typename T>
struct ReflPtr
{
    uintptr_t offset { 0 };

    inline const T* get() const
    {
        if (offset == 0)
        {
            return nullptr;
        }

        return reinterpret_cast<const T*>(reinterpret_cast<uintptr_t>(this) + offset);
    }

    inline explicit operator const T*() const
    {
        return get();
    }
};

template <typename T>
class ReflArray
{
public:
    i32         size { 0 };
    ReflPtr<T>  data;

    inline const T* begin() const
    {
        return data;
    }

    inline const T* end() const
    {
        return data + size;
    }

    inline const T& operator[](const i32 index) const
    {
        BEE_ASSERT(index < size);
        return data + index;
    }
};

class ReflString final : public ReflPtr<const char>
{
public:
    inline operator StringView() const // NOLINT
    {
        return StringView { get() };
    }
};

class ReflTypeRef
{
public:
    Type    type;
    u32     hash { 0 };

    ReflTypeRef() = default;

    explicit ReflTypeRef(const u32 type_hash)
        : hash(type_hash)
    {}

    Type get(); // NOLINT

    inline explicit operator Type()
    {
        return get();
    }
};


struct Attribute
{
    union Value // NOLINT
    {
        bool            boolean;
        int             integer;
        float           floating_point;
        ReflString      string;
        ReflTypeRef     type;
    };

    AttributeKind   kind { AttributeKind::invalid };
    u32             hash { 0 };
    ReflString      name;
    Value           value { false };
};


struct TemplateParameter
{
    u32         hash { 0 };
    ReflString  name;
    ReflString  type_name;
};

class SerializationBuilder;

struct Field
{
    using serialization_function_t = void(*)(SerializationBuilder*, void*);

    u32                         hash { 0 };
    size_t                      offset { 0 };
    Qualifier                   qualifier { Qualifier::none };
    StorageClass                storage_class { StorageClass::none };
    ReflString                  name;
    ReflTypeRef                 type;
    ReflArray<ReflTypeRef>      template_arguments;
    ReflArray<Attribute>        attributes;
    serialization_function_t    serializer_function { nullptr };
    i32                         version_added { 0 };
    i32                         version_removed { limits::max<i32>() };
    i32                         template_argument_in_parent { -1 };
};


class BEE_REFLECT(serializable, use_builder) BEE_CORE_API TypeInstance
{
public:
    using copier_t = void*(*)(Allocator* allocator, const void* other);
    using deleter_t = void(*)(Allocator* allocator, void* data);

    TypeInstance() = default;

    TypeInstance(const Type& type, void* data, Allocator* allocator, copier_t copier, deleter_t deleter);

    ~TypeInstance();

    TypeInstance(const TypeInstance& other);

    TypeInstance(TypeInstance&& other) noexcept;

    TypeInstance& operator=(const TypeInstance& other);

    TypeInstance& operator=(TypeInstance&& other) noexcept;

    inline bool is_valid() const
    {
        return allocator_ != nullptr && data_ != nullptr && type_.get() != nullptr && copier_ != nullptr && deleter_ != nullptr;
    }

    inline Allocator* allocator()
    {
        return allocator_;
    }

    inline const Type& type() const
    {
        return type_;
    }

    inline const void* data() const
    {
        return data_;
    }

    template <typename T>
    inline T* get()
    {
        return validate_type(get_type<T>()) ? static_cast<T*>(data_) : nullptr;
    }

    template <typename T>
    inline const T* get() const
    {
        return validate_type(get_type<T>()) ? static_cast<const T*>(data_) : nullptr;
    }

private:
    Allocator*  allocator_ { nullptr };
    void*       data_ { nullptr };
    Type        type_;
    copier_t    copier_ { nullptr };
    deleter_t   deleter_ { nullptr };

    void destroy();

    void copy_construct(const TypeInstance& other);

    void move_construct(TypeInstance& other) noexcept;

    bool validate_type(const Type& type) const;
};

template <typename T>
inline TypeInstance make_type_instance(Allocator* allocator)
{
    static auto static_deleter = [](Allocator* allocator, void* data)
    {
        auto as_type_ptr = static_cast<T*>(data);
        BEE_DELETE(allocator, as_type_ptr);
    };

    static auto static_copier = [](Allocator* allocator, const void* other) -> void*
    {
        return BEE_NEW(allocator, T)(*static_cast<const T*>(other));
    };

    return TypeInstance(get_type<T>(), BEE_NEW(allocator, T)(), allocator, static_copier, static_deleter);
}

template <>
inline TypeInstance make_type_instance<void>(Allocator* /* allocator */)
{
    return TypeInstance();
}


struct TypeInfo
{
    using create_instance_t = TypeInstance(*)(Allocator*);

    u32                             hash { 0 };
    size_t                          size { 0 };
    size_t                          alignment { 0 };
    TypeKind                        kind { TypeKind::unknown };
    ReflString                      name;
    i32                             serialized_version { 0 };
    SerializationFlags              serialization_flags { SerializationFlags::none };
    create_instance_t               create_instance { nullptr };
    ReflArray<TemplateParameter>    template_parameters;

    inline bool is(const TypeKind flag) const
    {
        if (flag == TypeKind::unknown)
        {
            return kind == TypeKind::unknown;
        }
        return (kind & flag) != TypeKind::unknown;
    }

    template <typename T>
    inline SpecializedType<T> as() const
    {
        BEE_ASSERT_F((T::kind_tag & kind) != TypeKind::unknown, "Invalid type cast");
        return SpecializedType<T>(reinterpret_cast<const T*>(this));
    }


    inline NamespaceRangeAdapter namespaces() const
    {
        return NamespaceRangeAdapter { Type(this) };
    }

    inline namespace_iterator namespaces_begin() const
    {
        return namespace_iterator { Type(this) };
    }

    inline namespace_iterator namespaces_end() const
    {
        return namespace_iterator(StringView(name.get() + str::length(name.get()), 0));
    }

    inline const char* unqualified_name() const
    {
        return name.get() + str::last_index_of(name, ':') + 1;
    }
};


template <TypeKind K>
struct SpecializedTypeInfo : public TypeInfo
{
    static constexpr TypeKind kind_tag = K;
};


struct ArrayTypeInfo final : public SpecializedTypeInfo<TypeKind::array>
{
    i32                             element_count { 0 };
    ReflTypeRef                     element_type;
    Field::serialization_function_t serializer_function { nullptr };
};

struct FundamentalTypeInfo final : public SpecializedTypeInfo<TypeKind::fundamental>
{
    FundamentalKind fundamental_kind { FundamentalKind::count };
};

struct EnumConstant
{
    ReflString  name;
    u32         hash { 0 };
    isize       value { 0 };
    ReflTypeRef underlying_type;
    bool        is_flag { false };
};

struct EnumTypeInfo final : public SpecializedTypeInfo<TypeKind::enum_decl>
{
    bool                    is_scoped { false };
    bool                    is_flags { false };
    ReflArray<EnumConstant> constants;
    ReflArray<Attribute>    attributes;
    ReflTypeRef             underlying_type;
};


struct FunctionTypeInvoker
{
    int     signature { 0 };
    void*   address { nullptr };

    FunctionTypeInvoker() noexcept = default;

    FunctionTypeInvoker(const int generated_signature, void* callable_address) noexcept
        : signature(generated_signature),
          address(callable_address)
    {}

    template <typename ReturnType, typename... Args>
    ReturnType invoke(Args&&... args)
    {
        BEE_ASSERT_F((get_signature<ReturnType, Args...>()) == signature, "invalid `invoke` signature: ReturnType and Args must match the signature of the FunctionType exactly - including cv and reference qualifications");
        return reinterpret_cast<ReturnType(*)(Args...)>(address)(std::forward<Args>(args)...);
    }

    template <typename ReturnType, typename... Args>
    ReturnType invoke(Args&&... args) const
    {
        BEE_ASSERT_F((get_signature<ReturnType, Args...>()) == signature, "invalid `invoke` signature: ReturnType and Args must match the signature of the FunctionType exactly - including cv and reference qualifications");
        return reinterpret_cast<ReturnType(*)(Args...)>(address)(std::forward<Args>(args)...);
    }

    template <typename ReturnType, typename... Args>
    static int get_signature()
    {
        static int value = next_signature();
        return value;
    }

    template <typename ReturnType, typename... Args>
    static FunctionTypeInvoker from(ReturnType(*callable)(Args...))
    {
        return FunctionTypeInvoker(FunctionTypeInvoker::get_signature<ReturnType, Args...>(), callable);
    }

private:
    static int next_signature() noexcept
    {
        static int next = 0;
        return next++;
    }
};


struct FunctionTypeInfo final : public SpecializedTypeInfo<TypeKind::function>
{
    StorageClass            storage_class { StorageClass::none };
    bool                    is_constexpr { false };
    Field                   return_value;
    ReflArray<Field>        parameters;
    ReflArray<Attribute>    attributes;
    FunctionTypeInvoker     invoker;

    template <typename ReturnType, typename... Args>
    ReturnType invoke(Args&&... args)
    {
        return invoker.invoke<ReturnType>(std::forward<Args>(args)...);
    }

    template <typename ReturnType, typename... Args>
    ReturnType invoke(Args&&... args) const
    {
        return invoker.invoke<ReturnType>(std::forward<Args>(args)...);
    }
};

struct RecordTypeInfo final : public SpecializedTypeInfo<TypeKind::record>
{
    ReflArray<Field>                        fields;
    ReflArray<ReflPtr<FunctionTypeInfo>>    functions;
    ReflArray<Attribute>                    attributes;
    ReflArray<ReflPtr<EnumTypeInfo>>        enums;
    ReflArray<ReflPtr<RecordTypeInfo>>      records;
    ReflArray<ReflTypeRef>                  base_records;
};

static constexpr u32 reflection_module_magic = 0x7CDD93B4;

struct ReflectionModule
{
    u32                             magic { 0 };
    ReflString                      name;
    ReflArray<ReflPtr<TypeInfo>>    all_types;
    ReflArray<RecordTypeInfo>       records;
    ReflArray<FunctionTypeInfo>     functions;
    ReflArray<EnumTypeInfo>         enums;
    ReflArray<ArrayTypeInfo>        arrays;
};

#define BEE_TYPE(T) using T = SpecializedType<T##Info>
BEE_TYPE(ArrayType);
BEE_TYPE(FundamentalType);
BEE_TYPE(EnumType);
BEE_TYPE(FunctionType);
BEE_TYPE(RecordType);
#undef BEE_TYPE

template <u32 Hash>
struct ComplexTypeTag
{
    static constexpr u32 hash = Hash;
};

template <typename T>
struct TypeTag
{
    using type = T;
};


inline constexpr bool operator==(const Type& lhs, const Type& rhs)
{
    return lhs->hash == rhs->hash;
}

inline constexpr bool operator!=(const Type& lhs, const Type& rhs)
{
    return lhs->hash != rhs->hash;
}


/*
 *******************************
 *
 * Reflection - get_type API
 *
 *******************************
 */
template <typename T>
Type get_type(const TypeTag<T>& tag);

template <typename T>
BEE_FORCE_INLINE Type get_type()
{
    return get_type(TypeTag<T>{});
}

template <typename ReflectedType, typename T>
BEE_FORCE_INLINE SpecializedType<T> get_type_as()
{
    return get_type<ReflectedType>()->template as<T>();
}

BEE_CORE_API u32 get_type_hash(const StringView& type_name);

// Thread-safe as long as nothing is calling `register_type`
BEE_CORE_API Type get_type(const u32 hash);

BEE_CORE_API void reflection_register_builtin_types();


/*
 ******************************
 *
 * Reflection module API
 *
 ******************************
 */
BEE_CORE_API const ReflectionModule* load_reflection_module(const Path& path); // relative to the exe dir

BEE_CORE_API void unload_reflection_module(const ReflectionModule* handle);

BEE_CORE_API const ReflectionModule* get_reflection_module(const StringView& name);


/*
 ******************************
 *
 * Reflection utilities
 *
 ******************************
 */
BEE_CORE_API const Attribute* find_attribute(const Type& type, const char* attribute_name);

BEE_CORE_API const Attribute* find_attribute(const Field& field, const char* attribute_name);

BEE_CORE_API const Attribute* find_attribute(const Type& type, const char* attribute_name, const AttributeKind kind);

BEE_CORE_API const Attribute* find_attribute(const Field& field, const char* attribute_name, const AttributeKind kind);

BEE_CORE_API const Attribute* find_attribute(const Type& type, const char* attribute_name, const AttributeKind kind, const Attribute::Value& value);

BEE_CORE_API const Attribute* find_attribute(const Field& field, const char* attribute_name, const AttributeKind kind, const Attribute::Value& value);

BEE_CORE_API const Field* find_field(const Span<Field>& fields, const char* name);

BEE_CORE_API const Field* find_field(const Span<Field>& fields, const StringView& name);

BEE_CORE_API const char* reflection_flag_to_string(const Qualifier qualifier);

BEE_CORE_API const char* reflection_flag_to_string(const StorageClass storage_class);

BEE_CORE_API const char* reflection_flag_to_string(const SerializationFlags serialization_flags);

BEE_CORE_API const char* reflection_flag_to_string(const TypeKind type_kind);

BEE_CORE_API const char* reflection_type_kind_to_code_string(const TypeKind type_kind);

BEE_CORE_API const char* reflection_attribute_kind_to_string(const AttributeKind attr_kind);

template <typename FlagType>
const char* reflection_dump_flags(const FlagType flag)
{
    static thread_local char buffer[4096];
    io::StringStream stream(buffer, static_array_length(buffer), 0);

    int count = 0;
    for_each_flag(flag, [&](const FlagType& f)
    {
        stream.write_fmt(" %s |", reflection_flag_to_string(f));
        ++count;
    });

    if (count == 0)
    {
        stream.write(reflection_flag_to_string(static_cast<FlagType>(0u)));
    }

    if (buffer[stream.size() - 1] == '|')
    {
        buffer[stream.size() - 1] = '\0';
    }
    else
    {
        stream.null_terminate();
    }

    // Skip the first space that occurs when getting multiple flags
    return count > 0 ? buffer + 1 : buffer;
}


template <typename T>
inline const EnumConstant& enum_to_type(const T& value)
{
    static EnumConstant default_constant {ReflString{}, 0, 0, Type(nullptr), false };

    auto type = get_type_as<T, EnumTypeInfo>();

    if (type->is(TypeKind::unknown))
    {
        return default_constant;
    }

    const auto const_index = find_index_if(type->constants, [&](const EnumConstant& c)
    {
        return c.value == static_cast<i64>(value);
    });

    return const_index >= 0 ? type->constants[const_index] : default_constant;
}

template <typename T>
inline void enum_to_string(io::StringStream* stream, const T& value)
{
    auto type = get_type_as<T, EnumTypeInfo>();
    if (!type->is_flags)
    {
        const auto const_index = find_index_if(type->constants, [&](const EnumConstant& c)
        {
            return c.value == static_cast<i64>(value);
        });

        if (const_index >= 0)
        {
            stream->write_fmt("%s", type->constants[const_index].name);
        }
        else
        {
            stream->write_fmt("%" PRId64, static_cast<i64>(value));
        }
    }
    else
    {
        int flag_count = count_bits(value);
        int current_flag = 0;
        for_each_flag(value, [&](const T& flag)
        {
            const auto const_index = find_index_if(type->constants, [&](const EnumConstant& c)
            {
                return c.value == static_cast<i64>(flag);
            });

            if (const_index >= 0)
            {
                stream->write_fmt("%s", type->constants[const_index].name);
            }
            else
            {
                stream->write_fmt("%" PRId64, static_cast<i64>(flag));
            }

            if (current_flag < flag_count - 1)
            {
                stream->write(" | ");
            }

            ++current_flag;
        });
    }
}

template <typename T>
inline String enum_to_string(const T& value, Allocator* allocator = system_allocator())
{
    String result(allocator);
    io::StringStream stream(&result);
    enum_to_string<T>(&stream, value);
    return std::move(result);
}

BEE_CORE_API isize enum_from_string(const EnumType type, const StringView& string);

template <typename T>
inline T enum_from_string(const StringView& string)
{
    return static_cast<T>(enum_from_string(get_type_as<T, EnumType>(), string));
}


} // namespace bee

#ifdef BEE_ENABLE_REFLECTION
#include "Bee.Core/ReflectedTemplates/Array.generated.inl"
#include "Bee.Core/ReflectedTemplates/String.generated.inl"
#endif // BEE_ENABLE_REFLECTION