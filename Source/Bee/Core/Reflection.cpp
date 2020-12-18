/*
 *  Reflection.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Reflection.hpp"
#include "Bee/Core/Containers/HashMap.hpp"


namespace bee {


template <typename T>
constexpr size_t alignof_helper()
{
    return alignof(T);
}

template <>
constexpr size_t alignof_helper<void>()
{
    return 0;
}

template <typename T>
constexpr size_t sizeof_helper()
{
    return sizeof(T);
}

template <>
constexpr size_t sizeof_helper<void>()
{
    return 0;
}


/*
 * Register all builtin fundamentals - these are registered as regular types
 */
#define BEE_BUILTIN_TYPE(builtin_type, function_name)                   \
    TypeInstance create_##function_name##_instance(Allocator* allocator)\
    {                                                                   \
        return make_type_instance<builtin_type>(allocator);             \
    }                                                                   \
    template <> BEE_CORE_API  Type get_type<builtin_type>(const TypeTag<builtin_type>& tag)      \
    {                                                                   \
        static FundamentalTypeInfo instance                             \
        {                                                               \
            get_type_hash(#builtin_type),                               \
            sizeof_helper<builtin_type>(),                              \
            alignof_helper<builtin_type>(),                             \
            #builtin_type,                                              \
            1,                                                          \
            create_##function_name##_instance,                          \
            FundamentalKind::function_name##_kind                       \
        };                                                              \
        return Type(&instance);                                         \
    }                                                                   \
    Type bee_get_type__##function_name() { return get_type<builtin_type>(); }

BEE_BUILTIN_TYPES

#undef BEE_BUILTIN_TYPE


struct NullPtrType final : public TypeSpec<TypeKind::unknown>
{
    NullPtrType() noexcept
        : TypeSpec(0, 0, 0, "std::nullptr_t", 0, SerializationFlags::none, nullptr)
    {}
};

static UnknownTypeInfo g_unknown_type{};
static NullPtrType g_nullptr_type{};


template <> BEE_CORE_API Type get_type<UnknownTypeInfo>(const TypeTag<UnknownTypeInfo>& tag)
{
    return Type(&g_unknown_type);
}

template <> BEE_CORE_API Type get_type<nullptr_t>(const TypeTag<nullptr_t>& tag)
{
    return Type(&g_nullptr_type);
}


/*
 ****************************************
 *
 * namespace_iterator - implementation
 *
 ****************************************
 */
namespace_iterator::namespace_iterator(const Type& type)
    : namespace_iterator(type->name)
{}

namespace_iterator::namespace_iterator(const StringView& fully_qualified_name)
    : current_(fully_qualified_name.c_str()),
      end_(fully_qualified_name.data() + fully_qualified_name.size()),
      size_(str::first_index_of(fully_qualified_name, "::"))
{
    if (size_ < 0)
    {
        // If there's no namespace then this should be equal to end()
        current_ = end_;
        size_ = 0;
    }
}

StringView namespace_iterator::operator*() const
{
    return StringView(current_, size_);
}

StringView namespace_iterator::operator->() const
{
    return StringView(current_, size_);
}

namespace_iterator& namespace_iterator::operator++()
{
    next_namespace();
    return *this;
}

bool namespace_iterator::operator==(const namespace_iterator& other) const
{
    return current_ == other.current_;
}

StringView namespace_iterator::view() const
{
    return StringView(current_, static_cast<i32>(end_ - current_));
}

void namespace_iterator::next_namespace()
{
    // Type name strings are guaranteed to be null-terminated
    const auto ns = str::first_index_of(view(), "::");
    if (ns > 0)
    {
        current_ += ns + 2;
    }

    const auto next_ns = str::first_index_of(view(), "::");

    if (next_ns > 0)
    {
        size_ = next_ns;
    }
    else
    {
        // either this is the last namespace and we've reached the unqualified type or this is an empty name
        current_ = end_;
        size_ = 0;
    }
}

namespace_iterator NamespaceRangeAdapter::begin() const
{
    return type->namespaces_begin();
}

namespace_iterator NamespaceRangeAdapter::end() const
{
    return type->namespaces_end();
}


/*
 ****************************************
 *
 * TypeInstance - implementation
 *
 ****************************************
 */
TypeInstance::TypeInstance(const Type& type, void* data, Allocator* allocator, copier_t copier, deleter_t deleter)
    : allocator_(allocator),
      data_(data),
      type_(type),
      copier_(copier),
      deleter_(deleter)
{}

TypeInstance::TypeInstance(const TypeInstance& other)
{
    if (this != &other)
    {
        copy_construct(other);
    }
}

TypeInstance::TypeInstance(TypeInstance&& other) noexcept
{
    move_construct(other);
}

TypeInstance::~TypeInstance()
{
    destroy();
    allocator_ = nullptr;
    data_ = nullptr;
    destruct(&type_);
    copier_ = nullptr;
    deleter_ = nullptr;
}

TypeInstance& TypeInstance::operator=(const TypeInstance& other)
{
    if (this != &other)
    {
        copy_construct(other);
    }

    return *this;
}

TypeInstance& TypeInstance::operator=(TypeInstance&& other) noexcept
{
    move_construct(other);
    return *this;
}

void TypeInstance::from(TypeInstance* other)
{
    destroy();
    data_ = other->copier_(allocator_, data_);
    type_ = other->type_;
    copier_ = other->copier_;
    deleter_ = other->deleter_;
}

void TypeInstance::destroy()
{
    if (data_ != nullptr)
    {
        BEE_ASSERT(allocator_ != nullptr);
        deleter_(allocator_, data_);
    }
}

void TypeInstance::copy_construct(const TypeInstance& other)
{
    destroy();
    allocator_ = other.allocator_;
    data_ = other.copier_(allocator_, other.data_);
    type_ = other.type_;
    copier_ = other.copier_;
    deleter_ = other.deleter_;
}

void TypeInstance::move_construct(TypeInstance& other) noexcept
{
    if (data_ != nullptr)
    {
        BEE_ASSERT(allocator_ != nullptr);
        deleter_(allocator_, data_);
    }

    allocator_ = other.allocator_;
    data_ = other.data_;
    type_ = other.type_;
    copier_ = other.copier_;
    deleter_ = other.deleter_;

    other.allocator_ = nullptr;
    other.data_ = nullptr;
    destruct(&other.type_);
    other.copier_ = nullptr;
    other.deleter_ = nullptr;
}

bool TypeInstance::validate_type(const Type& type) const
{
    BEE_ASSERT_F(type_.get() != nullptr, "TypeInstance: instance is not valid - no type information is available");
    BEE_ASSERT_F(data_ != nullptr, "TypeInstance: instance is null");
    return BEE_CHECK_F(type == type_, "TypeInstance: cannot cast from %s to %s", type_->name, type->name);
}

Type::Type()
    : type_(&g_unknown_type)
{}

bool Type::is_unknown() const
{
    return type_ == nullptr || type_ == &g_unknown_type;
}

template <typename T>
bool SpecializedType<T>::is_unknown() const
{
    return type_ == nullptr || type_ == &g_unknown_type;
}

/*
 ****************************************
 *
 * Reflection API - implementation
 *
 ****************************************
 */
struct ReflectionModule
{
    u32                         hash { 0 };
    i32                         type_count { 0 };
    const char*                 name { nullptr };
    const get_type_callback_t*  types { nullptr };
    u32*                        type_hashes { nullptr };
};

static DynamicHashMap<u32, get_type_callback_t> g_type_map;
static DynamicHashMap<u32, ReflectionModule*>   g_modules;

void register_type(const Type& type)
{
//    if (g_type_map.find(type->hash) == nullptr)
//    {
//        g_type_map.insert(type->hash, type.get());
//    }
}

void unregister_type(const Type& type)
{
//    if (g_type_map.find(type->hash) != nullptr)
//    {
//        g_type_map.erase(type->hash);
//    }
}

const ReflectionModule* create_reflection_module(const StringView& name, const i32 type_count, const u32* hashes, const get_type_callback_t* callbacks)
{
    const u32 hash = get_hash(name);

    if (BEE_FAIL_F(g_modules.find(hash) == nullptr, "Reflection module %" BEE_PRIsv " already exists", BEE_FMT_SV(name)))
    {
        return nullptr;
    }

    const size_t name_bytecount = name.size() * sizeof(char) + 1;
    const size_t types_bytecount = sizeof(get_type_callback_t) * (type_count + 1); // for null terminator
    const size_t type_hash_bytecount = sizeof(u32) * type_count;

    auto* mem = BEE_MALLOC(system_allocator(), sizeof(ReflectionModule) + name_bytecount + types_bytecount + type_hash_bytecount);
    auto* module = static_cast<ReflectionModule*>(mem);
    new (module) ReflectionModule{};

    auto* name_ptr = reinterpret_cast<char*>(static_cast<u8*>(mem) + sizeof(ReflectionModule));
    str::copy(name_ptr, name_bytecount, name);
    name_ptr[name_bytecount] = '\0';

    auto* types_ptr = reinterpret_cast<get_type_callback_t*>(static_cast<u8*>(mem) + sizeof(ReflectionModule) + name_bytecount);
    memcpy(types_ptr, callbacks, type_count * sizeof(get_type_callback_t));

    auto* type_hashes_ptr = reinterpret_cast<u32*>(static_cast<u8*>(mem) + sizeof(ReflectionModule) + name_bytecount + types_bytecount);
    memcpy(type_hashes_ptr, hashes, type_count * sizeof(u32));

    module->hash = hash;
    module->type_count = type_count;
    module->name = name_ptr;
    module->types = types_ptr;
    module->type_hashes = type_hashes_ptr;

    g_modules.insert(module->hash, module);

    for (int i = 0; i < type_count; ++i)
    {
        g_type_map.insert(hashes[i], callbacks[i]);
    }

    return module;
}

void destroy_reflection_module(const ReflectionModule* module)
{
    const u32 hash = module->hash;
    auto* stored_module = g_modules.find(hash);

    if (BEE_FAIL_F(stored_module != nullptr, "Reflection module %s was destroyed twice", module->name))
    {
        return;
    }

    for (int i = 0; i < module->type_count; ++i)
    {
        g_type_map.erase(module->type_hashes[i]);
    }

    BEE_FREE(system_allocator(), stored_module->value);
    g_modules.erase(hash);
}

const ReflectionModule* get_reflection_module(const StringView& name)
{
    auto* stored_module = g_modules.find(get_hash(name));
    BEE_ASSERT(stored_module != nullptr);
    return stored_module ? stored_module->value : nullptr;
}

u32 get_type_hash(const StringView& type_name)
{
    return get_hash(type_name.data(), type_name.size(), 0xb12e92e);
}

Type get_type(const u32 hash)
{
    auto* type = g_type_map.find(hash);
    if (type != nullptr)
    {
        return type->value();
    }

    return get_type<UnknownTypeInfo>();
}

Type get_type(const ReflectionModule* module, const i32 index)
{
    if (BEE_FAIL_F(index < module->type_count, "Invalid type index %d for reflection module %s", index, module->name))
    {
        return Type(&g_unknown_type);
    }

    return module->types[index]();
}

BEE_CORE_API void reflection_register_builtin_types()
{
#define BEE_BUILTIN_TYPE(builtin_type, function_name) { get_type<builtin_type>()->hash, bee_get_type__##function_name },

    struct GetTypeParams { u32 hash { 0 }; get_type_callback_t callback { nullptr }; };
    static GetTypeParams builtin_types[] { BEE_BUILTIN_TYPES };

    for (auto& type : builtin_types)
    {
        g_type_map.insert(type.hash, type.callback);
    }
}



isize enum_from_string(const EnumType& type, const StringView& string)
{
    if (!type->is_flags)
    {
        const auto const_hash = get_type_hash(string);
        const auto const_index = find_index_if(type->constants, [&](const EnumConstant& c)
        {
            return c.hash == const_hash;
        });

        if (const_index >= 0)
        {
            return type->constants[const_index].value;
        }

        return -1;
    }
    else
    {
        isize value = 0;

        const char* begin = string.begin();
        const char* end = begin;

        while (begin != string.end())
        {
            // skip alpha numeric chars to get end of token
            while (end != string.end() && !str::is_space(*end) && *end != '|')
            {
                ++end;
            }

            const auto const_hash = get_type_hash(StringView(begin, static_cast<i32>(end - begin)));
            const auto const_index = find_index_if(type->constants, [&](const EnumConstant& c)
            {
                return c.hash == const_hash;
            });

            if (const_index >= 0)
            {
                value |= type->constants[const_index].value;
            }

            // skip whitespace and '|' char to get beginning of next token
            while (end != string.end() && (str::is_space(*end) || *end == '|'))
            {
                ++end;
            }

            begin = end;
        }

        return value;
    }
}

/*
 * Comparing sorted integers is extremely cheap so a linear search is the fastest option here for smaller arrays
 * (less than 500 items) of small structs (i.e. Attribute is 24 bytes). It's still useful to keep the
 * attributes sorted by hash though as this still improves the linear search by ~2x based on the benchmarks done on
 * this data. Also, types usually have <= 5 attributes so doing i.e. a binary search would be a waste of CPU time
 */
const Attribute* find_attribute(const Span<Attribute>& attributes, const char* attribute_name, const AttributeKind kind, const Attribute::Value& value)
{
    const auto hash = get_type_hash(attribute_name);

    for (const Attribute& attr: attributes)
    {
        if (attr.hash == hash && attr.kind == kind && memcmp(&attr.value, &value, sizeof(Attribute::Value)) == 0)
        {
            return &attr;
        }
    }

    return nullptr;
}

const Attribute* find_attribute(const Span<Attribute>& attributes, const char* attribute_name, const AttributeKind kind)
{
    const auto hash = get_type_hash(attribute_name);

    for (const Attribute& attr: attributes)
    {
        if (attr.hash == hash && attr.kind == kind)
        {
            return &attr;
        }
    }

    return nullptr;
}

const Attribute* find_attribute(const Span<Attribute>& attributes, const char* attribute_name)
{
    const auto hash = get_type_hash(attribute_name);

    for (const Attribute& attr: attributes)
    {
        if (attr.hash == hash)
        {
            return &attr;
        }
    }

    return nullptr;
}

const Attribute* find_attribute(const Type& type, const char* attribute_name)
{
    switch(type->kind)
    {
        case TypeKind::class_decl:
        case TypeKind::struct_decl:
        case TypeKind::union_decl:
        {
            return find_attribute(type->as<RecordTypeInfo>()->attributes, attribute_name);
        }
        case TypeKind::enum_decl:
        {
            return find_attribute(type->as<EnumTypeInfo>()->attributes, attribute_name);
        }
        case TypeKind::function:
        {
            return find_attribute(type->as<FunctionTypeInfo>()->attributes, attribute_name);
        }
        default: break;
    }

    return nullptr;
}

const Attribute* find_attribute(const Type& type, const char* attribute_name, const AttributeKind kind)
{
    switch(type->kind)
    {
        case TypeKind::class_decl:
        case TypeKind::struct_decl:
        case TypeKind::union_decl:
        {
            return find_attribute(type->as<RecordTypeInfo>()->attributes, attribute_name, kind);
        }
        case TypeKind::enum_decl:
        {
            return find_attribute(type->as<EnumTypeInfo>()->attributes, attribute_name, kind);
        }
        case TypeKind::function:
        {
            return find_attribute(type->as<FunctionTypeInfo>()->attributes, attribute_name, kind);
        }
        default: break;
    }

    return nullptr;
}

const Attribute* find_attribute(const Type& type, const char* attribute_name, const AttributeKind kind, const Attribute::Value& value)
{
    switch(type->kind)
    {
        case TypeKind::class_decl:
        case TypeKind::struct_decl:
        case TypeKind::union_decl:
        {
            return find_attribute(type->as<RecordTypeInfo>()->attributes, attribute_name, kind, value);
        }
        case TypeKind::enum_decl:
        {
            return find_attribute(type->as<EnumTypeInfo>()->attributes, attribute_name, kind, value);
        }
        case TypeKind::function:
        {
            return find_attribute(type->as<FunctionTypeInfo>()->attributes, attribute_name, kind, value);
        }
        default: break;
    }

    return nullptr;
}

const Attribute* find_attribute(const Field& field, const char* attribute_name, const AttributeKind kind)
{
    return find_attribute(field.attributes, attribute_name, kind);
}

const Field* find_field(const Span<Field>& fields, const char* name)
{
    return find_field(fields, StringView(name));
}

const Field* find_field(const Span<Field>& fields, const StringView& name)
{
    const auto hash = get_type_hash(name);

    for (const Field& field : fields)
    {
        if (field.hash == hash)
        {
            return &field;
        }
    }

    return nullptr;
}


const char* reflection_flag_to_string(const Qualifier qualifier)
{
#define REFL_FLAG(x) case Qualifier::x: return "Qualifier::" #x

    switch (qualifier)
    {
        REFL_FLAG(none);
        REFL_FLAG(cv_const);
        REFL_FLAG(cv_volatile);
        REFL_FLAG(lvalue_ref);
        REFL_FLAG(rvalue_ref);
        REFL_FLAG(pointer);
        default:
        {
            BEE_UNREACHABLE("Missing Qualifier string representation");
        }
    }
#undef REFL_FLAG
}

const char* reflection_flag_to_string(const StorageClass storage_class)
{
#define REFL_FLAG(x) case StorageClass::x: return "StorageClass::" #x

    switch (storage_class)
    {
        REFL_FLAG(none);
        REFL_FLAG(auto_storage);
        REFL_FLAG(register_storage);
        REFL_FLAG(static_storage);
        REFL_FLAG(extern_storage);
        REFL_FLAG(thread_local_storage);
        REFL_FLAG(mutable_storage);
        default:
        {
            BEE_UNREACHABLE("Missing StorageClass string representation");
        }
    }
#undef REFL_FLAG
}

const char* reflection_flag_to_string(const SerializationFlags serialization_flags)
{
#define REFL_FLAG(x) case SerializationFlags::x: return "SerializationFlags::" #x

    switch (serialization_flags)
    {
        REFL_FLAG(none);
        REFL_FLAG(packed_format);
        REFL_FLAG(table_format);
        REFL_FLAG(uses_builder);
        REFL_FLAG(bytes);
        default:
        {
            BEE_UNREACHABLE("Missing SerializationFlags string representation");
        }
    }
#undef REFL_FLAG
}

const char* reflection_flag_to_string(const TypeKind type_kind)
{
#define TYPE_KIND(x) case TypeKind::x: return "TypeKind::" #x

    switch (type_kind)
    {
        TYPE_KIND(unknown);
        TYPE_KIND(class_decl);
        TYPE_KIND(struct_decl);
        TYPE_KIND(enum_decl);
        TYPE_KIND(union_decl);
        TYPE_KIND(template_decl);
        TYPE_KIND(field);
        TYPE_KIND(function);
        TYPE_KIND(fundamental);
        TYPE_KIND(array);
        default:
        {
            BEE_UNREACHABLE("Missing TypeKind string representation");
        }
    }
#undef TYPE_KIND
}

const char* reflection_type_kind_to_code_string(const TypeKind type_kind)
{
#define TYPE_KIND(x, str) if ((type_kind & TypeKind::x) != TypeKind::unknown) return str

    TYPE_KIND(class_decl, "class");
    TYPE_KIND(struct_decl, "struct");
    TYPE_KIND(enum_decl, "enum class");
    TYPE_KIND(union_decl, "union");

    return "";
#undef TYPE_KIND
}


BEE_TRANSLATION_TABLE(reflection_attribute_kind_to_string, AttributeKind, const char*, AttributeKind::invalid,
    "AttributeKind::boolean",           // boolean
    "AttributeKind::integer",           // integer
    "AttributeKind::floating_point",    // floating_point
    "AttributeKind::string",            // string
    "AttributeKind::type",              // type
)


} // namespace bee