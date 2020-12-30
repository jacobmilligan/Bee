/*
 *  Parse.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/ShaderPipeline/Parser/Lex.hpp"

#include "Bee/Gpu/Gpu.hpp"


namespace bee {


struct ShaderPipelineDescriptor;

/*
 ************************************
 *
 * Bee Shader Compiler - Nodes
 *
 ************************************
 */
struct ShaderFile
{
    struct Range
    {
        i32 offset { -1 };
        i32 size { -1 };

        inline bool empty() const
        {
            return offset < 0 || size <= 0;
        }
    };

    struct Pipeline // NOLINT
    {
        String                          name;
        PipelineStateDescriptor         desc; // contains everything except the shader handles
        i32                             shaders[ShaderStageIndex::count];

        Pipeline(const StringView& pipeline_name, Allocator* allocator)
            : name(pipeline_name, allocator)
        {}
    };

    struct SubShader
    {
        String                          name;
        String                          stage_entries[ShaderStageIndex::count];
        Range                           stage_code_ranges[ShaderStageIndex::count];
        i32                             resource_layout_count { 0 };
        ResourceBindingUpdateFrequency  resource_layout_frequencies[BEE_GPU_MAX_RESOURCE_LAYOUTS];

        SubShader(Allocator* allocator)
            : name(allocator)
        {
            for (auto& entry : stage_entries)
            {
                new (&entry) String(allocator);
            }
        }

        void set_entry(const i32 stage, const StringView& entry_name)
        {
            BEE_ASSERT(stage < static_array_length(stage_entries));
            stage_entries[stage].assign(entry_name);
        }
    };

    Allocator*                              allocator { nullptr };
    DynamicArray<Pipeline>                  pipelines;
    DynamicArray<SubShader>                 subshaders;
    DynamicArray<AttachmentDescriptor>      attachments;
    DynamicArray<u8>                        code;

    explicit ShaderFile(Allocator* new_allocator)
        : allocator(new_allocator),
          pipelines(new_allocator),
          subshaders(new_allocator),
          attachments(new_allocator),
          code(new_allocator)
    {}

    Pipeline& add_pipeline(const StringView& name)
    {
        pipelines.emplace_back(name, allocator);
        return pipelines.back();
    }

    SubShader& add_subshader(const StringView& subshader_name)
    {
        subshaders.emplace_back(allocator);
        subshaders.back().name.assign(subshader_name);
        return subshaders.back();
    }

    Range add_code(const u8* data, const i32 size)
    {
        Range range{};
        range.offset = code.size();
        range.size = size;

        code.append({ data, size });

        return range;
    }

    void get_shader_pipeline_descriptor(const Pipeline& pipeline, ShaderPipelineDescriptor* dst);
};

/*
 ************************************
 *
 * Bee Shader Compiler - Nodes
 *
 ************************************
 */
template <typename T>
struct BscNode
{
    StringView  identifier;
    T           data;

    BscNode() = default;

    template <typename... Args>
    explicit BscNode(const StringView assigned_identifier, Args&&... args)
        : identifier(assigned_identifier),
          data(BEE_FORWARD(args)...)
    {}
};

template <typename T>
using bsc_node_array_t = DynamicArray<BscNode<T>>;

struct BscShaderNode
{
    StringView                      code;
    StringView                      stages[ShaderStageIndex::count];
    i32                             resource_layout_count { 0 };
    ResourceBindingUpdateFrequency  resource_layouts[BEE_GPU_MAX_RESOURCE_LAYOUTS];
};

struct BscPipelineStateNode
{
    PrimitiveType               primitive_type { PrimitiveType::unknown };
    StringView                  raster_state;
    StringView                  multisample_state;
    StringView                  depth_stencil_state;
    StringView                  vertex_stage;
    StringView                  fragment_stage;

    StaticArray<StringView, BEE_GPU_MAX_ATTACHMENTS> color_blend_states;
};

struct BscModule
{
    Allocator*                                      allocator { nullptr };
    bsc_node_array_t<BscPipelineStateNode>          pipeline_states;
    bsc_node_array_t<RasterStateDescriptor>         raster_states;
    bsc_node_array_t<MultisampleStateDescriptor>    multisample_states;
    bsc_node_array_t<DepthStencilStateDescriptor>   depth_stencil_states;
    bsc_node_array_t<SamplerCreateInfo>             sampler_states;
    bsc_node_array_t<BscShaderNode>                 shaders;
    bsc_node_array_t<BlendStateDescriptor>          color_blend_states;

    explicit BscModule(Allocator* node_allocator)
        : allocator(node_allocator),
          pipeline_states(node_allocator),
          raster_states(node_allocator),
          multisample_states(node_allocator),
          depth_stencil_states(node_allocator),
          sampler_states(node_allocator),
          shaders(node_allocator),
          color_blend_states(node_allocator)
    {}
};


enum class BscResolveErrorCode
{
    invalid_parameters,
    undefined_symbol,
    too_many_shaders,
    incompatible_resource_layouts,
    incompatible_color_blend_states,
    none
};


struct BscResolveError
{
    BscResolveErrorCode code { BscResolveErrorCode::none };
    StringView          param;
    StringView          param2;

    BscResolveError() = default;

    explicit BscResolveError(const BscResolveErrorCode new_code)
        : code(new_code)
    {}

    BscResolveError(const BscResolveErrorCode new_code, const StringView& new_param)
        : code(new_code),
          param(new_param)
    {}

    BscResolveError(const BscResolveErrorCode new_code, const StringView& new_param, const StringView& new_second_param)
        : code(new_code),
          param(new_param),
          param2(new_second_param)
    {}

    explicit inline operator bool() const
    {
        return code == BscResolveErrorCode::none;
    }

    String to_string(Allocator* allocator = system_allocator()) const;
};

BscResolveError bsc_resolve_module(const BscModule& module, ShaderFile* output);

void bsc_log_resolve_error(const BscResolveError& error);


/**
 ************************************
 *
 * # BscParser
 *
 * Parses a BSC text file into an
 * intermediate representation
 * that can be used to later compile
 * many variants of different shaders
 *
 ************************************
 */
class BscParser
{
public:
    explicit BscParser() = default;

    bool parse(const StringView& source, BscModule* ast);

    inline const BscError& get_error() const
    {
        return error_;
    }

private:
    BscError    error_;

    struct Cursor
    {
        StringView  value;
        const char* current { nullptr };

        explicit Cursor(const StringView& new_value)
            : value(new_value),
              current(new_value.begin())
        {}

        inline void operator++()
        {
            if (current != value.end())
            {
                ++current;
            }
        }

        inline bool is_valid()
        {
            return current != value.end();
        }

        inline bool operator==(const char c) const
        {
            return *current == c;
        }

        inline bool operator!=(const char c) const
        {
            return *current != c;
        }
    };

    bool report_error(const BscErrorCode code, const BscLexer* lexer);

    bool parse_top_level_structure(BscLexer* lexer, BscModule* ast);

    bool parse_raster_state(BscLexer* lexer, BscNode<RasterStateDescriptor>* node);

    bool parse_multisample_state(BscLexer* lexer, BscNode<MultisampleStateDescriptor>* node);

    bool parse_depth_stencil_state(BscLexer* lexer, BscNode<DepthStencilStateDescriptor>* node);

    bool parse_pipeline_state(BscLexer* lexer, BscNode<BscPipelineStateNode>* node);

    bool parse_shader(BscLexer* lexer, BscNode<BscShaderNode>* node);

    bool parse_sampler_state(BscLexer* lexer, BscNode<SamplerCreateInfo>* node);

    bool parse_attachment(BscLexer* lexer, BscNode<AttachmentDescriptor>* node);

    bool parse_blend_state(BscLexer* lexer, BscNode<BlendStateDescriptor>* node);

    static bool parse_key(BscLexer* lexer, StringView* identifier);

    bool parse_fields(BscLexer* lexer, const RecordType& parent_type, void* parent_data);

    bool parse_value(BscLexer* lexer, const Field& field, u8* data);

    static bool parse_code(BscLexer* lexer, StringView* dst);

    bool parse_number(BscLexer* lexer, const BscTokenKind kind, const StringView& value, const FundamentalType& type, u8* data);

    bool parse_array(BscLexer* lexer, DynamicArray<StringView>* array);

    bool parse_array(BscLexer* lexer, StringView* array, const i32 capacity, i32* count);

    bool parse_array(BscLexer* lexer, const EnumType& enum_type, i32* array, const i32 capacity, i32* count);
};


} // namespace bee