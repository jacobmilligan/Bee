/*
 *  ReflectionV2.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Enum.hpp"
#include "Bee/Core/String.hpp"
#include "Bee/Core/Logger.hpp"
#include "Bee/Core/IO.hpp"


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
    fundamental             = 1u << 7u,
    array                   = 1u << 8u,
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


struct Type;
struct SerializationFunction;


class BEE_CORE_API namespace_iterator
{
public:
    using value_type    = StringView;
    using reference     = StringView&;
    using pointer       = StringView*;

    explicit namespace_iterator(const Type* type);

    explicit namespace_iterator(const StringView& fully_qualified_name);

    namespace_iterator(const namespace_iterator& other) = default;

    namespace_iterator& operator=(const namespace_iterator& other) = default;

    const StringView operator*() const;

    const StringView operator->() const;

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
    const Type* type { nullptr };

    namespace_iterator begin() const;

    namespace_iterator end() const;
};


struct NamespaceRangeFromNameAdapter
{
    const StringView& fully_qualified_name;

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


struct Attribute
{
    union Value
    {
        bool        boolean;
        int         integer;
        float       floating_point;
        const char* string;
        const Type* type;

        explicit Value(const bool b)
            : boolean(b)
        {}

        explicit Value(const int i)
            : integer(i)
        {}

        explicit Value(const float f)
            : floating_point(f)
        {}

        explicit Value(const char* s)
            : string(s)
        {}

        explicit Value(const Type* t)
            : type(t)
        {}
    };

    AttributeKind   kind { AttributeKind::invalid };
    u32             hash { 0 };
    const char*     name { nullptr };
    Value           value { false };

    Attribute() = default;

    Attribute(const AttributeKind new_kind, const u32 new_hash, const char* new_name, const Value& new_value)
        : kind(new_kind),
          hash(new_hash),
          name(new_name),
          value(new_value)
    {}
};


struct TemplateParameter
{
    u32         hash { 0 };
    const char* name { nullptr };
    const char* type_name { nullptr };

    TemplateParameter() = default;

    TemplateParameter(const u32 arg_name_hash, const char* arg_name, const char* arg_type_name)
        : hash(arg_name_hash),
          name(arg_name),
          type_name(arg_type_name)
    {}
};


struct Field
{
    u32                     hash { 0 };
    size_t                  offset { 0 };
    Qualifier               qualifier { Qualifier::none };
    StorageClass            storage_class { StorageClass::none };
    const char*             name { nullptr };
    const Type*             type { nullptr };
    Span<const Type*>       template_arguments;
    Span<Attribute>         attributes;
    SerializationFunction*  serializer_function { nullptr };
    i32                     version_added { 0 };
    i32                     version_removed { limits::max<i32>() };
    i32                     template_argument_in_parent { -1 };

    Field() = default;

    Field(
        const u32 new_hash,
        const size_t new_offset,
        const Qualifier new_qualifier,
        const StorageClass new_storage_class,
        const char* new_name,
        const Type* new_type,
        const Span<const Type*> new_template_args,
        const Span<Attribute> new_attributes,
        SerializationFunction* new_serializer_function,
        const i32 new_version_added = 0,
        const i32 new_version_removed = limits::max<i32>(),
        const i32 template_arg_index = -1
    ) : hash(new_hash),
        offset(new_offset),
        qualifier(new_qualifier),
        storage_class(new_storage_class),
        name(new_name),
        type(new_type),
        template_arguments(new_template_args),
        attributes(new_attributes),
        serializer_function(new_serializer_function),
        version_added(new_version_added),
        version_removed(new_version_removed),
        template_argument_in_parent(template_arg_index)
    {}
};


struct Type
{
    u32                     hash { 0 };
    size_t                  size { 0 };
    size_t                  alignment { 0 };
    TypeKind                kind { TypeKind::unknown };
    const char*             name { nullptr };
    i32                     serialized_version { 0 };
    SerializationFlags      serialization_flags { SerializationFlags::none };
    Span<TemplateParameter> template_parameters;

    Type() = default;

    Type(
        const u32 new_hash,
        const size_t new_size,
        const size_t new_alignment,
        const TypeKind new_kind,
        const char* new_name,
        const i32 new_serialized_version,
        const SerializationFlags new_serialization_flags
    ) : hash(new_hash),
        size(new_size),
        alignment(new_alignment),
        kind(new_kind),
        name(new_name),
        serialized_version(new_serialized_version),
        serialization_flags(new_serialization_flags)
    {}

    Type(
        const u32 new_hash,
        const size_t new_size,
        const size_t new_alignment,
        const TypeKind new_kind,
        const char* new_name,
        const i32 new_serialized_version,
        const SerializationFlags new_serialization_flags,
        const Span<TemplateParameter> new_template_parameters
    ) : hash(new_hash),
        size(new_size),
        alignment(new_alignment),
        kind(new_kind),
        name(new_name),
        serialized_version(new_serialized_version),
        serialization_flags(new_serialization_flags),
        template_parameters(new_template_parameters)
    {}

    inline bool is(const TypeKind flag) const
    {
        return (kind & flag) != TypeKind::unknown;
    }

    template <typename T>
    inline const T* as() const
    {
        static_assert(std::is_base_of<Type, T>::value, "`T` must derive from bee::Type");
        BEE_ASSERT_F((T::static_kind & kind) != TypeKind::unknown, "Invalid type cast");
        return reinterpret_cast<const T*>(this);
    }

    inline NamespaceRangeAdapter namespaces() const
    {
        return NamespaceRangeAdapter { this };
    }

    inline namespace_iterator namespaces_begin() const
    {
        return namespace_iterator(this);
    }

    inline namespace_iterator namespaces_end() const
    {
        return namespace_iterator(StringView(name + str::length(name), 0));
    }

    inline const char* unqualified_name() const
    {
        return name + str::last_index_of(name, ':') + 1;
    }
};

template <typename T>
inline const T* typekind_cast(const Type* type)
{
    static_assert(std::is_base_of<Type, T>::value, "`T` must derive from bee::Type");
    BEE_ASSERT_F((T::static_kind & type->kind) != TypeKind::unknown, "Invalid type cast");
    return reinterpret_cast<const T*>(type);
}


template <TypeKind Kind>
struct TypeSpec : public Type
{
    static constexpr TypeKind static_kind = Kind;

    TypeSpec() = default;

    TypeSpec(
        const u32 new_hash,
        const size_t new_size,
        const size_t new_alignment,
        const char* new_name,
        const i32 new_serialized_version,
        const SerializationFlags new_serialization_flags
    ) : Type(new_hash, new_size, new_alignment, Kind, new_name, new_serialized_version, new_serialization_flags)
    {}

    TypeSpec(
        const u32 new_hash,
        const size_t new_size,
        const size_t new_alignment,
        const TypeKind specialized_kind,
        const char* new_name,
        const i32 new_serialized_version,
        const SerializationFlags new_serialization_flags
    ) : Type(new_hash, new_size, new_alignment, specialized_kind, new_name, new_serialized_version, new_serialization_flags)
    {}

    TypeSpec(
        const u32 new_hash,
        const size_t new_size,
        const size_t new_alignment,
        const TypeKind specialized_kind,
        const char* new_name,
        const i32 new_serialized_version,
        const SerializationFlags new_serialization_flags,
        const Span<TemplateParameter> new_template_parameters
    ) : Type(new_hash, new_size, new_alignment, Kind | TypeKind::template_decl, new_name, new_serialized_version, new_serialization_flags, new_template_parameters)
    {
        BEE_ASSERT(!new_template_parameters.empty());
    }

    TypeSpec(
        const u32 new_hash,
        const size_t new_size,
        const size_t new_alignment,
        const char* new_name,
        const i32 new_serialized_version,
        const SerializationFlags new_serialization_flags,
        const Span<TemplateParameter> new_template_parameters
    ) : TypeSpec(new_hash, new_size, new_alignment, Kind, new_name, new_serialized_version, new_serialization_flags, new_template_parameters)
    {}
};


struct ArrayType final : public TypeSpec<TypeKind::array>
{
    i32         element_count { 0 };
    const Type* element_type { nullptr };

    ArrayType() = default;

    ArrayType(
        const u32 new_hash,
        const size_t new_size,
        const size_t new_alignment,
        const char* new_name,
        const i32 new_serialized_version,
        const SerializationFlags new_serialization_flags,
        const i32 count,
        const Type* type
    ) : TypeSpec(new_hash, new_size, new_alignment, new_name, new_serialized_version, new_serialization_flags),
        element_count(count),
        element_type(type)
    {}
};


struct FundamentalType final : public TypeSpec<TypeKind::fundamental>
{
    FundamentalKind fundamental_kind { FundamentalKind::count };

    FundamentalType() = default;

    FundamentalType(
        const u32 new_hash,
        const size_t new_size,
        const size_t new_alignment,
        const char* new_name,
        const i32 new_serialized_version,
        const FundamentalKind new_fundamental_kind
    ) : TypeSpec(new_hash, new_size, new_alignment, new_name, new_serialized_version, SerializationFlags::none),
        fundamental_kind(new_fundamental_kind)
    {}
};



struct EnumConstant
{
    const char* name { nullptr };
    i64         value { 0 };
    const Type* underlying_type { nullptr };

    EnumConstant() = default;

    EnumConstant(const char* new_name, const i64 new_value, const Type* new_underlying_type)
        : name(new_name),
          value(new_value),
          underlying_type(new_underlying_type)
    {}
};

struct EnumType final : public TypeSpec<TypeKind::enum_decl>
{
    bool                is_scoped { false };
    Span<EnumConstant>  constants;
    Span<Attribute>     attributes;

    EnumType() = default;

    EnumType(
        const u32 new_hash,
        const size_t new_size,
        const size_t new_alignment,
        const char* new_name,
        const i32 new_serialized_version,
        const SerializationFlags new_serialization_flags,
        const bool scoped,
        const Span<EnumConstant> new_constants,
        const Span<Attribute> new_attributes
    ) : TypeSpec(new_hash, new_size, new_alignment, new_name, new_serialized_version, new_serialization_flags),
        is_scoped(scoped),
        constants(new_constants),
        attributes(new_attributes)
    {}
};


struct FunctionTypeInvoker
{
    int     signature { 0 };
    void*   address { nullptr };

    FunctionTypeInvoker() = default;

    FunctionTypeInvoker(const int generated_signature, void* callable_address)
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


struct FunctionType final : public TypeSpec<TypeKind::function>
{
    StorageClass        storage_class { StorageClass::none };
    bool                is_constexpr { false };
    Field               return_value;
    Span<Field>         parameters;
    Span<Attribute>     attributes;
    FunctionTypeInvoker invoker;

    using TypeSpec::TypeSpec;

    FunctionType() = default;

    FunctionType(
        const u32 new_hash,
        const size_t new_size,
        const size_t new_alignment,
        const char* new_name,
        const i32 new_serialized_version,
        const SerializationFlags new_serialization_flags,
        const StorageClass new_storage_class,
        const bool make_constexpr,
        const Field& new_return_value,
        const Span<Field> new_parameters,
        const Span<Attribute> new_attributes,
        FunctionTypeInvoker callable_invoker
    ) : TypeSpec(new_hash, new_size, new_alignment, new_name, new_serialized_version, new_serialization_flags),
        storage_class(new_storage_class),
        is_constexpr(make_constexpr),
        return_value(new_return_value),
        parameters(new_parameters),
        attributes(new_attributes),
        invoker(callable_invoker)
    {}

    FunctionType(
        const u32 new_hash,
        const size_t new_size,
        const size_t new_alignment,
        const char* new_name,
        const i32 new_serialized_version,
        const SerializationFlags new_serialization_flags,
        const Span<TemplateParameter> new_template_parameters,
        const StorageClass new_storage_class,
        const bool make_constexpr,
        const Field& new_return_value,
        const Span<Field> new_parameters,
        const Span<Attribute> new_attributes,
        FunctionTypeInvoker callable_invoker
    ) : TypeSpec(new_hash, new_size, new_alignment, new_name, new_serialized_version, new_serialization_flags, new_template_parameters),
        storage_class(new_storage_class),
        is_constexpr(make_constexpr),
        return_value(new_return_value),
        parameters(new_parameters),
        attributes(new_attributes),
        invoker(callable_invoker)
    {}

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


struct RecordType final : public TypeSpec<TypeKind::record>
{
    Span<Field>                 fields;
    Span<FunctionType>          functions;
    Span<Attribute>             attributes;
    Span<const EnumType*>       enums;
    Span<const RecordType*>     records;

    using TypeSpec::TypeSpec;

    RecordType() = default;

    RecordType(
        const u32 new_hash,
        const size_t new_size,
        const size_t new_alignment,
        const TypeKind new_kind,
        const char* new_name,
        const i32 new_serialized_version,
        const SerializationFlags new_serialization_flags,
        const Span<Field>& new_fields,
        const Span<FunctionType> new_functions,
        const Span<Attribute> new_attributes,
        const Span<const EnumType*> nested_enums,
        const Span<const RecordType*> nested_records
    ) : TypeSpec(new_hash, new_size, new_alignment, new_kind, new_name, new_serialized_version, new_serialization_flags),
        fields(new_fields),
        functions(new_functions),
        attributes(new_attributes),
        enums(nested_enums),
        records(nested_records)
    {}

    RecordType(
        const u32 new_hash,
        const size_t new_size,
        const size_t new_alignment,
        const TypeKind new_kind,
        const char* new_name,
        const i32 new_serialized_version,
        const SerializationFlags new_serialization_flags,
        const Span<TemplateParameter> new_template_parameters,
        const Span<Field>& new_fields,
        const Span<FunctionType> new_functions,
        const Span<Attribute> new_attributes,
        const Span<const EnumType*> nested_enums,
        const Span<const RecordType*> nested_records
    ) : TypeSpec(new_hash, new_size, new_alignment, new_kind, new_name, new_serialized_version, new_serialization_flags, new_template_parameters),
        fields(new_fields),
        functions(new_functions),
        attributes(new_attributes),
        enums(nested_enums),
        records(nested_records)
    {}
};

struct UnknownType final : public TypeSpec<TypeKind::unknown>
{
    UnknownType()
        : TypeSpec(0, 0, 0, "bee::UnknownType", 0, SerializationFlags::none)
    {}
};


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


extern void reflection_init();

template <typename T>
const Type* get_type(const TypeTag<T>& tag);

template <typename T>
inline const Type* get_type()
{
    return get_type(TypeTag<T>{});
}

BEE_CORE_API u32 get_type_hash(const StringView& type_name);

BEE_CORE_API const Type* get_type(const u32 hash);

template <typename ReflectedType, typename T>
inline const T* get_type_as()
{
    return get_type<ReflectedType>()->template as<T>();
}

BEE_CORE_API void reflection_register_builtin_types();

BEE_CORE_API void register_type(const Type* type);

BEE_CORE_API const Attribute* find_attribute(const Type* type, const char* attribute_name);

BEE_CORE_API const Attribute* find_attribute(const Field& field, const char* attribute_name);

BEE_CORE_API const Attribute* find_attribute(const Type* type, const char* attribute_name, const AttributeKind kind);

BEE_CORE_API const Attribute* find_attribute(const Field& field, const char* attribute_name, const AttributeKind kind);

BEE_CORE_API const Attribute* find_attribute(const Type* type, const char* attribute_name, const AttributeKind kind, const Attribute::Value& value);

BEE_CORE_API const Attribute* find_attribute(const Field& field, const char* attribute_name, const AttributeKind kind, const Attribute::Value& value);

BEE_CORE_API const Field* find_field(const Span<Field>& fields, const char* name);

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

    // Skip the first space that occurs when getting multiple flags
    return count > 0 ? buffer + 1 : buffer;
}


} // namespace bee

#if BEE_CONFIG_ENABLE_REFLECTION == 1
#include "ReflectedTemplates/Array.generated.inl"
#endif // BEE_CONFIG_ENABLE_REFLECTION
