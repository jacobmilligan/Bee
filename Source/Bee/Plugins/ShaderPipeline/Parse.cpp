/*
 *  Parse.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/Plugins/ShaderPipeline/Parse.hpp"

namespace bee {


template <typename T>
struct ResolveOrError
{
    bool is_error { false };

    union
    {
        const T*        ptr;
        BscResolveError error;
    };

    explicit inline operator bool() const
    {
        return !is_error;
    }

    inline const T* operator->() const
    {
        BEE_ASSERT(!is_error);
        return ptr;
    }

    inline const T& operator*() const
    {
        BEE_ASSERT(!is_error);
        return *ptr;
    }
};

template <typename T>
ResolveOrError<T> bsc_find_node(const bsc_node_array_t<T>& array, const StringView& identifier)
{
    const auto index = find_index_if(array, [&](const BscNode<T>& value)
    {
        return value.identifier == identifier;
    });

    ResolveOrError<T> result{};
    result.is_error = index < 0;

    if (result.is_error)
    {
        result.error.code = BscResolveErrorCode::undefined_symbol;
        result.error.param = identifier;
    }
    else
    {
        result.ptr = &array[index].data;
    }

    return result;
}

template <typename T>
i32 bsc_find_node_index(const bsc_node_array_t<T>& array, const StringView& identifier)
{
    return find_index_if(array, [&](const BscNode<T>& value)
    {
        return value.identifier == identifier;
    });
}
/*
 ************************************
 *
 * Resolve BscModule into a series
 * of ShaderPipeline objects
 *
 ************************************
 */
struct SubpassAttachmentConversion
{
    u32*                            count { nullptr };
    u32*                            indices { nullptr };
    i32                             indices_size { 0 };
    const DynamicArray<StringView>& identifiers;

    template <i32 Size>
    SubpassAttachmentConversion(u32& new_count, u32(&new_indices)[Size], const DynamicArray<StringView>& to_convert)
        : count(&new_count),
          indices(new_indices),
          indices_size(Size),
          identifiers(to_convert)
    {}
};

BscResolveError bsc_convert_subpass_attachments(const BscRenderPassNode& pass, SubpassAttachmentConversion* conversion)
{
    BEE_ASSERT(conversion->indices_size >= conversion->identifiers.size());

    *conversion->count = 0;

    for (const auto& attachment : conversion->identifiers)
    {
        const auto index = bsc_find_node_index(pass.attachments, attachment);
        if (index < 0)
        {
            return BscResolveError(BscResolveErrorCode::undefined_symbol, attachment);
        }

        conversion->indices[*conversion->count] = sign_cast<u32>(index);
        ++(*conversion->count);
    }

    return BscResolveError{};
}

BscResolveError bsc_convert_subpass(const BscRenderPassNode& pass, const i32 subpass_index, SubPassDescriptor* output)
{
    const auto& subpass = pass.subpasses[subpass_index].data;

    SubpassAttachmentConversion conversions[] = {
        { output->input_attachment_count, output->input_attachments, subpass.input_attachments },
        { output->color_attachment_count, output->color_attachments, subpass.color_attachments },
        { output->preserve_attachment_count, output->preserve_attachments, subpass.preserve_attachments },
        { output->resolve_attachment_count, output->resolve_attachments, subpass.resolve_attachments }
    };

    BscResolveError err{};

    for (auto& conversion : conversions)
    {
        err = bsc_convert_subpass_attachments(pass, &conversion);
        if (!err)
        {
            return err;
        }
    }

    return err;
}

BscResolveError bsc_add_pass(const BscRenderPassNode& input, Shader* shader)
{
    const auto& pass = shader->add_pass(input.attachments.size(), input.subpasses.size());

    for (int i = 0; i < input.attachments.size(); ++i)
    {
        shader->attachments[pass.attachments.offset + i] = input.attachments[i].data;
    }

    BscResolveError err{};

    for (int i = 0; i < input.subpasses.size(); ++i)
    {
        err = bsc_convert_subpass(input, i, &shader->subpasses[pass.subpasses.offset + i]);
        if (!err)
        {
            return err;
        }
    }

    return err;
}

BscResolveError bsc_resolve_module(const BscModule& module, Shader* output)
{
    if (output == nullptr)
    {
        return BscResolveError(BscResolveErrorCode::invalid_parameters);
    }

    const BscShaderNode* stages[gpu_shader_stage_count] { nullptr };
    StringView stage_names[gpu_shader_stage_count];

    // TODO(Jacob): ensure multiple-defined symbols are not possible - use a symbol table for resolving this
    DynamicHashMap<StringView, i32> symbol_map(temp_allocator());

    for (int i = 0; i < module.pipeline_states.size(); ++i)
    {
        const auto& in = module.pipeline_states[i].data;

        output->pipelines.emplace_back();
        auto& out_pipeline = output->pipelines.back();

        // Resolve all pipeline symbols
        const auto pass_index = bsc_find_node_index(module.render_passes, in.render_pass);
        if (pass_index < 0)
        {
            return { BscResolveErrorCode::undefined_symbol, in.render_pass };
        }

        const auto& pass = module.render_passes[pass_index].data;

        auto subpass_index = bsc_find_node_index(pass.subpasses, in.subpass);
        if (subpass_index < 0)
        {
            return { BscResolveErrorCode::undefined_symbol, in.subpass };
        }

        // Raster state - not required
        if (!in.raster_state.empty())
        {
            auto raster_state = bsc_find_node(module.raster_states, in.raster_state);
            if (!raster_state)
            {
                return raster_state.error;
            }

            out_pipeline.info.raster_state = *raster_state;
        }

        // Multisample state - not required
        if (!in.multisample_state.empty())
        {
            auto multisample_state = bsc_find_node(module.multisample_states, in.multisample_state);
            if (!multisample_state)
            {
                return multisample_state.error;
            }

            out_pipeline.info.multisample_state = *multisample_state;
        }

        // Depth stencil state - not required
        if (!in.depth_stencil_state.empty())
        {
            auto depth_stencil_state = bsc_find_node(module.depth_stencil_states, in.depth_stencil_state);
            if (!depth_stencil_state)
            {
                return depth_stencil_state.error;
            }

            out_pipeline.info.depth_stencil_state = *depth_stencil_state;
        }

        // Resolve all the shader stages
        memset(stage_names, 0, sizeof(StringView) * static_array_length(stage_names));
        memset(stages, 0, sizeof(const BscShaderNode*) * static_array_length(stages));

        stage_names[underlying_t(ShaderStageIndex::vertex)] = in.vertex_stage;
        stage_names[underlying_t(ShaderStageIndex::fragment)] = in.fragment_stage;

        for (const auto stage : enumerate(stage_names))
        {
            if (stage.value.empty())
            {
                out_pipeline.shaders[stage.index] = -1;
                continue;
            }

            // find the shader node in the module
            auto shader_node_index = bsc_find_node_index(module.shaders, stage.value);
            if (shader_node_index < 0)
            {
                return { BscResolveErrorCode::undefined_symbol, stage.value };
            }

            // resolve the stage and entry strings from the parsed form
            const auto& shader = module.shaders[shader_node_index];
            auto* subshader_index = symbol_map.find(shader.identifier);

            if (subshader_index == nullptr)
            {
                /*
                 * New subshader resolution - resolve the name, entries, and resource objects if needed
                 * Code ranges dont need to be resolved becayse they're assigned after compiling and reflecting the HLSL
                 */
                subshader_index = symbol_map.insert(shader.identifier, output->subshaders.size());

                output->subshaders.emplace_back();

                auto& subshader = output->subshaders.back();
                subshader.name = shader.identifier;

                for (int entry_index = 0; entry_index < static_array_length(shader.data.stages); ++entry_index)
                {
                    subshader.stage_entries[entry_index] = shader.data.stages[entry_index];
                }
            }

            out_pipeline.shaders[stage.index] = subshader_index->value;
        }

        // Resolve the resource layouts
        for (int l = 0; l < in.resource_layout_count; ++l)
        {
            const auto layout = bsc_find_node(module.resource_layouts, in.resource_layouts[l]);
            if (!layout)
            {
                return layout.error;
            }

            out_pipeline.info.resource_layouts[out_pipeline.info.resource_layout_count++] = *layout;
        }

        // Generate the ShaderPass
        auto error = bsc_add_pass(pass, output);
        if (!error)
        {
            return error;
        }

        // Generate the ShaderPipeline
        out_pipeline.pass = pass_index;
        out_pipeline.info.subpass_index = sign_cast<u32>(subpass_index);
        out_pipeline.info.primitive_type = in.primitive_type;
        // TODO(Jacob): color blend states, resource layouts, and push constants
    }

    return BscResolveError{};
}

String BscResolveError::to_string(Allocator* allocator) const
{
    switch (code)
    {
        case BscResolveErrorCode::invalid_parameters:
        {
            return String("BSC: invalid parameters given to resolve symbols", allocator);
        }
        case BscResolveErrorCode::undefined_symbol:
        {
            return str::format(allocator, "BSC: undefined symbol: %" BEE_PRIsv, BEE_FMT_SV(param));
        }
        case BscResolveErrorCode::none:
        {
            return String(allocator);
        }
        default:
        {
            break;
        }
    }

    return String("BSC: unknown error", allocator);
}


/*
 ************************************
 *
 * BscParser - implementation
 *
 ************************************
 */

bool BscParser::report_error(const BscErrorCode code, const BscLexer* lexer)
{
    error_.code = code;
    error_.text = lexer->current();
    error_.error_char = *lexer->current();
    error_.line = lexer->line();
    error_.column = lexer->column();
    return false;
}

bool BscParser::parse(const StringView& source, BscModule* ast)
{
    BscLexer lexer(source);

    while (lexer.is_valid())
    {
        if (!parse_top_level_structure(&lexer, ast))
        {
            break;
        }
    }

    if (lexer.get_error().code != BscErrorCode::none)
    {
        error_ = lexer.get_error();
    }

    return error_.code == BscErrorCode::none;
}

bool BscParser::parse_top_level_structure(BscLexer* lexer, BscModule* ast)
{
    BscToken tok{};
    if (!lexer->consume(&tok))
    {
        return false;
    }

    const auto kind = tok.kind;

    if (!lexer->consume_as(BscTokenKind::identifier, &tok))
    {
        return false;
    }

    StringView ident(tok.begin, tok.end);

    if (!lexer->consume_as(BscTokenKind::open_bracket, &tok))
    {
        return false;
    }

    bool success = false;

    switch (kind)
    {
        case BscTokenKind::RenderPass:
        {
            ast->render_passes.emplace_back(ident, ast->allocator);
            success = parse_render_pass(lexer, &ast->render_passes.back());
            break;
        }
        case BscTokenKind::RasterState:
        {
            ast->raster_states.emplace_back(ident);
            success = parse_raster_state(lexer, &ast->raster_states.back());
            break;
        }
        case BscTokenKind::MultisampleState:
        {
            ast->multisample_states.emplace_back(ident);
            success = parse_multisample_state(lexer, &ast->multisample_states.back());
            break;
        }
        case BscTokenKind::DepthStencilState:
        {
            ast->depth_stencil_states.emplace_back(ident);
            success = parse_depth_stencil_state(lexer, &ast->depth_stencil_states.back());
            break;
        }
        case BscTokenKind::PipelineState:
        {
            ast->pipeline_states.emplace_back(ident);
            success = parse_pipeline_state(lexer, &ast->pipeline_states.back());
            break;
        }
        case BscTokenKind::Shader:
        {
            ast->shaders.emplace_back(ident);
            success = parse_shader(lexer, &ast->shaders.back());
            break;
        }
        case BscTokenKind::SamplerState:
        {
            ast->sampler_states.emplace_back(ident);
            success = parse_sampler_state(lexer, &ast->sampler_states.back());
            break;
        }
        case BscTokenKind::ResourceLayout:
        {
            ast->resource_layouts.emplace_back(ident);
            success = parse_resource_layout(lexer, &ast->resource_layouts.back());
            break;
        }
        default:
        {
            success = report_error(BscErrorCode::invalid_object_type, lexer);
            break;
        }
    }

    if (!success)
    {
        return false;
    }

    return lexer->consume_as(BscTokenKind::close_bracket, &tok);
}

bool BscParser::parse_render_pass(BscLexer* lexer, BscNode<BscRenderPassNode>* node)
{
    BscToken tok{};

    while (lexer->peek(&tok))
    {
        if (tok.kind == BscTokenKind::close_bracket)
        {
            break;
        }

        if (!lexer->consume(&tok))
        {
            return false;
        }

        const auto kind = tok.kind;

        if (!lexer->consume_as(BscTokenKind::identifier, &tok))
        {
            return false;
        }

        StringView ident(tok.begin, tok.end);

        if (!lexer->consume_as(BscTokenKind::open_bracket, &tok))
        {
            return false;
        }

        // parse attachments and subpasses
        switch (kind)
        {
            case BscTokenKind::Attachment:
            {
                node->data.attachments.emplace_back(ident);
                if (!parse_attachment(lexer, &node->data.attachments.back()))
                {
                    return false;
                }
                break;
            }

            case BscTokenKind::SubPass:
            {
                node->data.subpasses.emplace_back(ident);
                if (!parse_subpass(lexer, &node->data.subpasses.back()))
                {
                    return false;
                }
                break;
            }

            default:
            {
                return report_error(BscErrorCode::invalid_object_type, lexer);
            }
        }

        if (!lexer->consume_as(BscTokenKind::close_bracket, &tok))
        {
            return false;
        }
    }

    return true;
}

bool BscParser::parse_raster_state(BscLexer* lexer, BscNode<RasterStateDescriptor>* node)
{
    return parse_fields(lexer, get_type_as<RasterStateDescriptor, RecordType>(), &node->data);
}

bool BscParser::parse_multisample_state(BscLexer* lexer, BscNode<MultisampleStateDescriptor>* node)
{
    return parse_fields(lexer, get_type_as<MultisampleStateDescriptor, RecordType>(), &node->data);
}

bool BscParser::parse_depth_stencil_state(BscLexer* lexer, BscNode<DepthStencilStateDescriptor>* node)
{
    return parse_fields(lexer, get_type_as<DepthStencilStateDescriptor, RecordType>(), &node->data);
}

bool BscParser::parse_sampler_state(BscLexer* lexer, BscNode<SamplerCreateInfo>* node)
{
    return parse_fields(lexer, get_type_as<SamplerCreateInfo, RecordType>(), &node->data);
}

bool BscParser::parse_resource_layout(BscLexer* lexer, BscNode<ResourceLayoutDescriptor>* node)
{
    BscToken tok{};
    StringView key{};

    while (lexer->peek(&tok))
    {
        if (tok.kind == BscTokenKind::close_bracket)
        {
            break;
        }

        if (sign_cast<i32>(node->data.resource_count) >= static_array_length(node->data.resources))
        {
            return report_error(BscErrorCode::too_many_fields, lexer);
        }

        if (!parse_key(lexer, &key))
        {
            return false;
        }

        if (!lexer->consume_as(BscTokenKind::open_bracket, &tok))
        {
            return false;
        }

        auto& desc = node->data.resources[node->data.resource_count];
        desc.binding = node->data.resource_count++;

        if (!parse_fields(lexer, get_type_as<ResourceDescriptor, RecordType>(), &desc))
        {
            return false;
        }

        if (!lexer->consume_as(BscTokenKind::close_bracket, &tok))
        {
            return false;
        }
    }

    return true;
}

bool BscParser::parse_pipeline_state(BscLexer* lexer, BscNode<BscPipelineStateNode>* node)
{
    BscToken tok{};
    StringView key{};
    StringView value{};

    while (lexer->peek(&tok))
    {
        if (tok.kind == BscTokenKind::close_bracket)
        {
            break;
        }

        if (!parse_key(lexer, &key))
        {
            return false;
        }

        if (key == "resource_layouts")
        {
            const auto layouts_success = parse_array(
                lexer,
                node->data.resource_layouts,
                static_array_length(node->data.resource_layouts),
                &node->data.resource_layout_count
            );

            if (!layouts_success)
            {
                return false;
            }

            continue;
        }

        if (!lexer->consume_as(BscTokenKind::identifier, &tok))
        {
            return false;
        }

        value = StringView(tok.begin, tok.end);

        if (key == "primitive_type")
        {
            auto constant = enum_from_string(get_type_as<PrimitiveType, EnumType>(), value);

            if (constant < 0)
            {
                return report_error(BscErrorCode::invalid_field_value, lexer);
            }

            node->data.primitive_type = static_cast<PrimitiveType>(constant);
        }
        else if (key == "render_pass")
        {
            node->data.render_pass = value;
        }
        else if (key == "subpass")
        {
            node->data.subpass = value;
        }
        else if (key == "raster_state")
        {
            node->data.raster_state = value;
        }
        else if (key == "multisample_state")
        {
            node->data.multisample_state = value;
        }
        else if (key == "depth_stencil_state")
        {
            node->data.depth_stencil_state = value;
        }
        else if (key == "vertex_stage")
        {
            node->data.vertex_stage = value;
        }
        else if (key == "fragment_stage")
        {
            node->data.fragment_stage = value;
        }
        else
        {
            return report_error(BscErrorCode::invalid_field_value, lexer);
        }
    }

    return true;
}

bool BscParser::parse_shader(BscLexer* lexer, BscNode<BscShaderNode>* node)
{
    BscToken tok{};
    StringView key{};

    while (lexer->peek(&tok))
    {
        if (tok.kind == BscTokenKind::close_bracket)
        {
            break;
        }

        if (!parse_key(lexer, &key))
        {
            return false;
        }

        if (key == "vertex")
        {
            if (!lexer->consume_as(BscTokenKind::identifier, &tok))
            {
                return false;
            }

            node->data.stages[underlying_t(ShaderStageIndex::vertex)] = StringView(tok.begin, tok.end);
        }
        else if (key == "fragment")
        {
            if (!lexer->consume_as(BscTokenKind::identifier, &tok))
            {
                return false;
            }

            node->data.stages[underlying_t(ShaderStageIndex::fragment)] = StringView(tok.begin, tok.end);
        }
        else if (key == "code")
        {
            if (!parse_code(lexer, &node->data.code))
            {
                return false;
            }
        }
        else
        {
            return report_error(BscErrorCode::invalid_field_value, lexer);
        }
    }

    return true;
}

bool BscParser::parse_attachment(BscLexer* lexer, BscNode<AttachmentDescriptor>* node)
{
    return parse_fields(lexer, get_type_as<AttachmentDescriptor, RecordType>(), &node->data);
}

bool BscParser::parse_subpass(BscLexer* lexer, BscNode<BscSubPassNode>* node)
{
    BscToken tok{};
    StringView key{};

    while (lexer->peek(&tok))
    {
        if (tok.kind == BscTokenKind::close_bracket)
        {
            break;
        }

        if (!parse_key(lexer, &key))
        {
            return false;
        }

        if (key == "depth_stencil")
        {
            if (!lexer->consume_as(BscTokenKind::identifier, &tok))
            {
                return false;
            }
            node->data.depth_stencil = StringView(tok.begin, tok.end);
        }
        else
        {
            DynamicArray<StringView>* array = nullptr;

            if (key == "input_attachments")
            {
                array = &node->data.input_attachments;
            }
            else if (key == "color_attachments")
            {
                array = &node->data.color_attachments;
            }
            else if (key == "preserve_attachments")
            {
                array = &node->data.preserve_attachments;
            }
            else if (key == "resolve_attachments")
            {
                array = &node->data.resolve_attachments;
            }
            else
            {
                return report_error(BscErrorCode::invalid_object_field, lexer);
            }

            return parse_array(lexer, array);
        }
    }

    return true;
}

bool BscParser::parse_key(BscLexer* lexer, StringView* identifier)
{
    BscToken tok{};

    if (!lexer->consume_as(BscTokenKind::identifier, &tok))
    {
        return false;
    }

    *identifier = StringView(tok.begin, tok.end);

    return lexer->consume_as(BscTokenKind::colon, &tok);
}

bool BscParser::parse_fields(bee::BscLexer* lexer, const RecordTypeRef& parent_type, void* parent_data)
{
    BscToken tok{};
    StringView key{};

    while (lexer->peek(&tok))
    {
        if (tok.kind == BscTokenKind::close_bracket)
        {
            break;
        }

        if (!parse_key(lexer, &key))
        {
            return false;
        }

        auto field = find_field(parent_type->fields, key);
        if (field == nullptr)
        {
            return report_error(BscErrorCode::invalid_object_field, lexer);
        }

        if (!parse_value(lexer, *field, static_cast<u8*>(parent_data) + field->offset))
        {
            return false;
        }
    }

    return true;
}

bool BscParser::parse_value(BscLexer* lexer, const Field& field, u8* data)
{
    BscToken tok{};
    if (!lexer->consume(&tok))
    {
        return false;
    }

    switch (tok.kind)
    {
        case BscTokenKind::open_bracket:
        {
            if (!parse_fields(lexer, field.type->as<RecordType>(), data))
            {
                return false;
            }

            return lexer->consume_as(BscTokenKind::close_bracket, &tok);
        }
        case BscTokenKind::identifier:
        {
            StringView ident(tok.begin, tok.end);

            if (field.type->is(TypeKind::enum_decl))
            {
                auto as_enum = field.type->as<EnumType>();
                const auto constant = enum_from_string(as_enum, ident);

                if (constant < 0)
                {
                    return report_error(BscErrorCode::invalid_field_value, lexer);
                }

                memcpy(data, &constant, as_enum->underlying_type->size);
            }
            else
            {
                if (field.type != get_type<StringView>())
                {
                    return report_error(BscErrorCode::invalid_field_value, lexer);
                }

                *reinterpret_cast<StringView*>(data) = ident;
            }

            break;
        }
        case BscTokenKind::bool_true:
        {
            *reinterpret_cast<bool*>(data) = true;
            break;
        }
        case BscTokenKind::bool_false:
        {
            *reinterpret_cast<bool*>(data) = false;
            break;
        }
        case BscTokenKind::signed_int:
        case BscTokenKind::unsigned_int:
        case BscTokenKind::floating_point:
        {
            return parse_number(lexer, tok.kind, { tok.begin, tok.end }, field.type->as<FundamentalType>(), data);
        }
        case BscTokenKind::string_literal:
        {
            if (field.type != get_type<StringView>())
            {
                return report_error(BscErrorCode::invalid_object_field, lexer);
            }

            *reinterpret_cast<StringView*>(data) = StringView(tok.begin, tok.end);
            break;
        }
        default:
        {
            return report_error(BscErrorCode::invalid_object_type, lexer);
        }
    }

    return true;
}

bool BscParser::parse_code(BscLexer* lexer, StringView* dst)
{
    BscToken tok{};

    if (!lexer->consume_as(BscTokenKind::open_bracket, &tok))
    {
        return false;
    }

    const auto begin = lexer->current();

    int scope_count = 0;
    while (scope_count >= 0)
    {
        if (!lexer->advance_valid())
        {
            return false;
        }

        if (*lexer->current() == '{')
        {
            ++scope_count;
            continue;
        }

        if (*lexer->current() == '}')
        {
            --scope_count;
            continue;
        }
    }

    *dst = StringView(begin, lexer->current());
    return lexer->consume_as(BscTokenKind::close_bracket, &tok);
}

bool BscParser::parse_number(BscLexer* lexer, const BscTokenKind kind, const StringView& value, const FundamentalTypeRef& type, u8* data)
{
    static thread_local char temp_buffer[64];

    if (value.size() > static_array_length(temp_buffer))
    {
        return report_error(BscErrorCode::number_too_long, lexer);
    }

    str::copy(temp_buffer, static_array_length(temp_buffer), value);

    if (kind == BscTokenKind::floating_point)
    {
        if (type == get_type<float>())
        {
            char* end = nullptr;
            const auto float32 = strtof(temp_buffer, &end);

            if (*end != '\0')
            {
                return report_error(BscErrorCode::invalid_number_format, lexer);
            }

            memcpy(data, &float32, sizeof(float));
        }
        else
        {
            char* end = nullptr;
            const auto float64 = strtod(temp_buffer, &end);

            if (*end != '\0')
            {
                return report_error(BscErrorCode::invalid_number_format, lexer);
            }

            memcpy(data, &float64, sizeof(double));
        }
    }
    else
    {
        char* end = nullptr;

        if (kind == BscTokenKind::signed_int)
        {
            const auto int64 = strtoll(temp_buffer, &end, 10);
            if (*end != '\0')
            {
                return report_error(BscErrorCode::invalid_number_format, lexer);
            }
            memcpy(data, &int64, type->size);
        }
        else
        {
            const auto uint64 = strtoull(temp_buffer, &end, 10);
            if (*end != '\0')
            {
                return report_error(BscErrorCode::invalid_number_format, lexer);
            }
            memcpy(data, &uint64, type->size);
        }
    }

    return true;
}

bool BscParser::parse_array(BscLexer* lexer, DynamicArray<StringView>* array)
{
    BscToken tok{};

    if (!lexer->consume_as(BscTokenKind::open_square_bracket, &tok))
    {
        return false;
    }

    while (lexer->consume_as(BscTokenKind::identifier, &tok))
    {
        array->emplace_back(tok.begin, tok.end);

        if (!lexer->consume(&tok))
        {
            return false;
        }

        if (tok.kind == BscTokenKind::close_square_bracket)
        {
            return true;
        }

        if (tok.kind != BscTokenKind::comma)
        {
            return report_error(BscErrorCode::unexpected_character, lexer);
        }
    }

    if (lexer->get_error().code != BscErrorCode::none)
    {
        return false;
    }

    return lexer->consume_as(BscTokenKind::close_square_bracket, &tok);
}

bool BscParser::parse_array(BscLexer* lexer, StringView* array, const i32 capacity, i32* count)
{
    BscToken tok{};

    if (!lexer->consume_as(BscTokenKind::open_square_bracket, &tok))
    {
        return false;
    }

    *count = 0;

    while (lexer->consume_as(BscTokenKind::identifier, &tok))
    {
        if (*count >= capacity)
        {
            return report_error(BscErrorCode::array_too_large, lexer);
        }

        array[*count] = StringView(tok.begin, tok.end);
        ++(*count);

        if (!lexer->consume(&tok))
        {
            return false;
        }

        if (tok.kind == BscTokenKind::close_square_bracket)
        {
            return true;
        }

        if (tok.kind != BscTokenKind::comma)
        {
            return report_error(BscErrorCode::unexpected_character, lexer);
        }

    }

    if (lexer->get_error().code != BscErrorCode::none)
    {
        return false;
    }

    return lexer->consume_as(BscTokenKind::close_square_bracket, &tok);
}


} // namespace bee