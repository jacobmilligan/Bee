/*
 *  Parse.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Plugins/DefaultAssets/ShaderCompiler/Lex.hpp"
#include "Bee/Graphics/GPU.hpp"
#include "Bee/Graphics/Shader.hpp"


namespace bee {

/**
 ************************************
 *
 * # BscModule
 *
 * The result of parsing a .bsc file
 *
 ************************************
 */
//struct BscShaderVariant
//{
//    BscTarget       target { BscTarget::none };
//    ShaderStage     stage { ShaderStage::unknown };
//    FixedArray<u8>  code;
//};
template <typename T>
struct BscNode
{
    StringView  identifier;
    T           data;

    BscNode() = default;

    template <typename... Args>
    explicit BscNode(const StringView assigned_identifier, Args&&... args)
        : identifier(assigned_identifier),
          data(std::forward<Args>(args)...)
    {}
};

template <typename T>
using bsc_node_array_t = DynamicArray<BscNode<T>>;

struct BscShaderNode
{
    StringView  stages[gpu_shader_stage_count];
    StringView  code;
};

struct BscSubPassNode
{
    DynamicArray<StringView>    input_attachments;
    DynamicArray<StringView>    color_attachments;
    DynamicArray<StringView>    resolve_attachments;
    DynamicArray<StringView>    preserve_attachments;
    StringView                  depth_stencil;

    explicit BscSubPassNode(Allocator* allocator = system_allocator())
        : input_attachments(allocator),
          color_attachments(allocator),
          resolve_attachments(allocator),
          preserve_attachments(allocator)
    {}
};

struct BscRenderPassNode
{
    bsc_node_array_t<AttachmentDescriptor>  attachments;
    bsc_node_array_t<BscSubPassNode>        subpasses;

    explicit BscRenderPassNode(Allocator* allocator = system_allocator())
        : attachments(allocator),
          subpasses(allocator)
    {}
};

struct BscPipelineStateNode
{
    PrimitiveType   primitive_type { PrimitiveType::unknown };
    StringView      render_pass;
    StringView      subpass;
    StringView      raster_state;
    StringView      multisample_state;
    StringView      depth_stencil_state;
    StringView      vertex_stage;
    StringView      fragment_stage;
};

struct BscModule
{
    Allocator*                                      allocator { nullptr };
    bsc_node_array_t<BscPipelineStateNode>          pipeline_states;
    bsc_node_array_t<BscRenderPassNode>             render_passes;
    bsc_node_array_t<RasterStateDescriptor>         raster_states;
    bsc_node_array_t<MultisampleStateDescriptor>    multisample_states;
    bsc_node_array_t<DepthStencilStateDescriptor>   depth_stencil_states;
    bsc_node_array_t<BscShaderNode>                 shaders;

    explicit BscModule(Allocator* node_allocator = system_allocator())
        : allocator(node_allocator),
          pipeline_states(node_allocator),
          render_passes(node_allocator),
          raster_states(node_allocator),
          multisample_states(node_allocator),
          depth_stencil_states(node_allocator)
    {}
};


enum class BscResolveErrorCode
{
    invalid_parameters,
    undefined_symbol,
    too_many_shaders,
    none
};


struct BscResolveError
{
    BscResolveErrorCode code { BscResolveErrorCode::none };
    StringView          param;

    BscResolveError() = default;

    explicit BscResolveError(const BscResolveErrorCode new_code)
        : code(new_code)
    {}

    BscResolveError(const BscResolveErrorCode new_code, const StringView& new_param)
        : code(new_code),
          param(new_param)
    {}

    explicit inline operator bool() const
    {
        return code == BscResolveErrorCode::none;
    }

    String to_string(Allocator* allocator = system_allocator()) const;
};

BscResolveError bsc_resolve_module(const BscModule& module, Shader* output);

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
    explicit BscParser(Allocator* allocator = system_allocator());

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

    bool parse_render_pass(BscLexer* lexer, BscNode<BscRenderPassNode>* node);

    bool parse_raster_state(BscLexer* lexer, BscNode<RasterStateDescriptor>* node);

    bool parse_multisample_state(BscLexer* lexer, BscNode<MultisampleStateDescriptor>* node);

    bool parse_depth_stencil_state(BscLexer* lexer, BscNode<DepthStencilStateDescriptor>* node);

    bool parse_pipeline_state(BscLexer* lexer, BscNode<BscPipelineStateNode>* node);

    bool parse_shader(BscLexer* lexer, BscNode<BscShaderNode>* node);

    bool parse_attachment(BscLexer* lexer, BscNode<AttachmentDescriptor>* node);

    bool parse_subpass(BscLexer* lexer, BscNode<BscSubPassNode>* node);

    static bool parse_key(BscLexer* lexer, StringView* identifier);

    bool parse_fields(BscLexer* lexer, const RecordType* parent_type, void* parent_data);

    bool parse_value(BscLexer* lexer, const Field& field, u8* data);

    static bool parse_code(BscLexer* lexer, StringView* dst);

    bool parse_number(BscLexer* lexer, const BscTokenKind kind, const StringView& value, const FundamentalType* type, u8* data);

    bool parse_array(BscLexer* lexer, DynamicArray<StringView>* array);
};


} // namespace bee