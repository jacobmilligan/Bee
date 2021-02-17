/*
 *  Parse.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/ShaderPipeline/Parser/Parse.hpp"
#include "Bee/ShaderPipeline/Compiler.hpp"

#include "Bee/Core/Containers/HashMap.hpp"


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
BscResolveError bsc_resolve_module(const BscModule& module, ShaderFile* output)
{
    if (output == nullptr)
    {
        return BscResolveError(BscResolveErrorCode::invalid_parameters);
    }

    StringView stage_names[ShaderStageIndex::count];

    // TODO(Jacob): ensure multiple-defined symbols are not possible - use a symbol table for resolving this
    DynamicHashMap<StringView, i32> symbol_map(module.allocator);

    // Resolve all pipeline symbols
    for (int i = 0; i < module.pipeline_states.size(); ++i)
    {
        const auto& in = module.pipeline_states[i].data;
        auto& out_pipeline = output->add_pipeline(module.pipeline_states[i].identifier);

        // Raster state - not required
        if (!in.raster_state.empty())
        {
            auto raster_state = bsc_find_node(module.raster_states, in.raster_state);
            if (!raster_state)
            {
                return raster_state.error;
            }

            out_pipeline.desc.raster_state = *raster_state;
        }

        // Multisample state - not required
        if (!in.multisample_state.empty())
        {
            auto multisample_state = bsc_find_node(module.multisample_states, in.multisample_state);
            if (!multisample_state)
            {
                return multisample_state.error;
            }

            out_pipeline.desc.multisample_state = *multisample_state;
        }

        // Depth stencil state - not required
        if (!in.depth_stencil_state.empty())
        {
            auto depth_stencil_state = bsc_find_node(module.depth_stencil_states, in.depth_stencil_state);
            if (!depth_stencil_state)
            {
                return depth_stencil_state.error;
            }

            out_pipeline.desc.depth_stencil_state = *depth_stencil_state;
        }

        // Color blend state - required. Must be == attachment count
        if (!in.color_blend_states.empty())
        {
            for (const auto& ident : in.color_blend_states)
            {
                auto blend_state = bsc_find_node(module.color_blend_states, ident);
                if (!blend_state)
                {
                    return blend_state.error;
                }

                const int index = out_pipeline.desc.color_blend_states.size;
                ++out_pipeline.desc.color_blend_states.size;
                out_pipeline.desc.color_blend_states[index] = *blend_state;
            }
        }

        // Resolve all the shader stages
        memset(stage_names, 0, sizeof(StringView) * static_array_length(stage_names));

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
                 * Code ranges dont need to be resolved because they're assigned after compiling and reflecting the HLSL
                 */
                subshader_index = symbol_map.insert(shader.identifier, output->subshaders.size());

                auto& subshader = output->add_subshader(shader.identifier);

                for (int entry_index = 0; entry_index < static_array_length(shader.data.stages); ++entry_index)
                {
                    subshader.set_entry(entry_index, shader.data.stages[entry_index]);
                }

                // resource update frequencies
                subshader.update_frequencies.append(shader.data.update_frequencies.const_span());

                // resolve sampler identifiers to sampler create info indices
                for (const auto& sampler : shader.data.samplers)
                {
                    const int src_index = find_index_if(module.sampler_states, [&](const BscNode<SamplerCreateInfo>& info)
                    {
                        return info.identifier == sampler.data;
                    });

                    if (src_index < 0)
                    {
                       return { BscResolveErrorCode::undefined_symbol, sampler.data };
                    }

                    // add_sampler handles any duplicate samplers via the src_index param
                    const int ref_index = output->add_sampler(src_index, module.sampler_states[src_index].data);
                    subshader.add_sampler_ref(sampler.identifier, ref_index);
                }
            }

            out_pipeline.shaders[stage.index] = subshader_index->value;
        }

        // Generate the ShaderPipeline
        out_pipeline.desc.primitive_type = in.primitive_type;

        // TODO(Jacob): push constants
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
        case BscResolveErrorCode::incompatible_resource_layouts:
        {
            return str::format(allocator, "BSC: incompatible shaders assigned to pipeline: %" BEE_PRIsv, BEE_FMT_SV(param));
        }
        case BscResolveErrorCode::incompatible_color_blend_states:
        {
            return str::format(allocator, "BSC: color_blend_states.size in pipeline '%" BEE_PRIsv "' must be the same as color_attachments.size in subpass '%" BEE_PRIsv "'", BEE_FMT_SV(param), BEE_FMT_SV(param2));
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

    if (source.empty())
    {
        return report_error(BscErrorCode::unexpected_eof, &lexer);
    }

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
            ast->shaders.emplace_back(ident, ast->allocator);
            success = parse_shader(lexer, &ast->shaders.back());
            break;
        }
        case BscTokenKind::SamplerState:
        {
            ast->sampler_states.emplace_back(ident);
            success = parse_sampler_state(lexer, &ast->sampler_states.back());
            break;
        }
        case BscTokenKind::BlendState:
        {
            ast->color_blend_states.emplace_back(ident);
            success = parse_blend_state(lexer, &ast->color_blend_states.back());
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

bool BscParser::parse_raster_state(BscLexer* lexer, BscNode<RasterStateDescriptor>* node)
{
    return parse_fields(lexer, get_type_as<RasterStateDescriptor, RecordTypeInfo>(), &node->data);
}

bool BscParser::parse_multisample_state(BscLexer* lexer, BscNode<MultisampleStateDescriptor>* node)
{
    return parse_fields(lexer, get_type_as<MultisampleStateDescriptor, RecordTypeInfo>(), &node->data);
}

bool BscParser::parse_depth_stencil_state(BscLexer* lexer, BscNode<DepthStencilStateDescriptor>* node)
{
    return parse_fields(lexer, get_type_as<DepthStencilStateDescriptor, RecordTypeInfo>(), &node->data);
}

bool BscParser::parse_sampler_state(BscLexer* lexer, BscNode<SamplerCreateInfo>* node)
{
    return parse_fields(lexer, get_type_as<SamplerCreateInfo, RecordTypeInfo>(), &node->data);
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

        if (key == "color_blend_states")
        {
            const bool success = parse_array(
                lexer,
                node->data.color_blend_states.data,
                node->data.color_blend_states.capacity,
                &node->data.color_blend_states.size
            );

            if (!success)
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
            auto constant = enum_from_string(get_type_as<PrimitiveType, EnumTypeInfo>(), value);

            if (constant < 0)
            {
                return report_error(BscErrorCode::invalid_field_value, lexer);
            }

            node->data.primitive_type = static_cast<PrimitiveType>(constant);
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
        else if (key == "color_blend_states")
        {
            node->data.color_blend_states.size = 0;
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
        else if (key == "samplers")
        {
            if (!parse_array(lexer, &node->data.samplers))
            {
                return false;
            }
        }
        else if (key == "update_frequencies")
        {
            if (!parse_update_frequencies(lexer, &node->data.update_frequencies))
            {
                return false;
            }
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

bool BscParser::parse_blend_state(BscLexer* lexer, BscNode<BlendStateDescriptor>* node)
{
    return parse_fields(lexer, get_type_as<BlendStateDescriptor, RecordTypeInfo>(), &node->data);
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

bool BscParser::parse_fields(bee::BscLexer* lexer, const RecordType& parent_type, void* parent_data)
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

        auto* field = find_field(parent_type->fields, key);
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
            if (!parse_fields(lexer, field.type->as<RecordTypeInfo>(), data))
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
                auto as_enum = field.type->as<EnumTypeInfo>();
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
            return parse_number(lexer, tok.kind, { tok.begin, tok.end }, field.type->as<FundamentalTypeInfo>(), data);
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

bool BscParser::parse_update_frequencies(BscLexer* lexer, DynamicArray<ShaderFile::UpdateFrequency>* dst)
{
    BscToken tok{};

    if (!lexer->consume_as(BscTokenKind::open_bracket, &tok))
    {
        return false;
    }

    StringView ident{};
    while (parse_key(lexer, &ident))
    {
        // parse key (i.e. `layout_0`) and validate format
        const int index = str::first_index_of(ident, "layout_");
        if (index != 0)
        {
            return report_error(BscErrorCode::invalid_layout_name, lexer);
        }

        // grab the layout index
        int layout = -1;
        if (!str::to_i32(str::substring(ident, index + sizeof("layout_")), &layout))
        {
            return report_error(BscErrorCode::invalid_layout_name, lexer);
        }

        if (!lexer->consume_as(BscTokenKind::identifier, &tok))
        {
            return false;
        }

        // Parse the frequency identifier to a valid enum value
        const auto freq = enum_from_string<ResourceBindingUpdateFrequency>(StringView(tok.begin, tok.end));
        if (static_cast<isize>(freq) < 0)
        {
            return report_error(BscErrorCode::invalid_field_value, lexer);
        }

        if (dst->size() == BEE_GPU_MAX_RESOURCE_LAYOUTS)
        {
            return report_error(BscErrorCode::array_too_large, lexer);
        }

        dst->emplace_back();
        dst->back().layout = layout;
        dst->back().frequency = freq;

        if (!lexer->peek(&tok))
        {
            return false;
        }

        if (tok.kind == BscTokenKind::close_bracket)
        {
            break;
        }
    }

    if (lexer->get_error().code != BscErrorCode::none)
    {
        return false;
    }

    return lexer->consume_as(BscTokenKind::close_bracket, &tok);
}

bool BscParser::parse_code(BscLexer* lexer, StringView* dst)
{
    BscToken tok{};

    if (!lexer->consume_as(BscTokenKind::open_bracket, &tok))
    {
        return false;
    }

    const auto* const begin = lexer->current();

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

bool BscParser::parse_number(BscLexer* lexer, const BscTokenKind kind, const StringView& value, const FundamentalType& type, u8* data)
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

bool BscParser::parse_array(BscLexer* lexer, bsc_node_array_t<StringView>* array)
{
    BscToken tok{};

    if (!lexer->consume_as(BscTokenKind::open_bracket, &tok))
    {
        return false;
    }

    StringView key{};
    while (parse_key(lexer, &key))
    {
        if (!lexer->consume_as(BscTokenKind::identifier, &tok))
        {
            return false;
        }

        array->emplace_back(key);
        array->back().data = StringView(tok.begin, tok.end);

        if (!lexer->peek(&tok))
        {
            return false;
        }

        if (tok.kind == BscTokenKind::close_bracket)
        {
            break;
        }
    }

    return lexer->consume_as(BscTokenKind::close_bracket, &tok);
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

bool BscParser::parse_array(BscLexer* lexer, const EnumType& enum_type, i32* array, const i32 capacity, i32* count)
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

        array[*count] = sign_cast<i32>(enum_from_string(enum_type, StringView(tok.begin, tok.end)));
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

// ShaderFile
void ShaderFile::copy_to_asset(const Pipeline& pipeline, Shader* dst)
{
    dst->name = pipeline.name.view();
    memcpy(&dst->pipeline_desc, &pipeline.desc, sizeof(PipelineStateDescriptor));

    dst->update_frequencies.size = pipeline.desc.resource_layouts.size;
    for (auto& freq : dst->update_frequencies)
    {
        freq = ResourceBindingUpdateFrequency::persistent;
    }

    for (auto index : enumerate(pipeline.shaders))
    {
        if (index.value < 0)
        {
            continue;
        }

        const ShaderStageIndex stage_index(index.index);
        const int subshader_index = index.value;
        auto& subshader = subshaders[subshader_index];
        auto& stage = dst->stages[dst->stages.size];

        stage.flags = stage_index.to_flags();
        stage.entry = subshader.stage_entries[stage_index].view();
        const u8* code_begin = code.data() + subshader.stage_code_ranges[stage_index].offset;
        const size_t code_size = sign_cast<size_t>(subshader.stage_code_ranges[stage_index].size);
        stage.code.resize(code_size);
        stage.code.copy(0, code_begin, code_begin + code_size);

        // Update frequencies
        for (const auto& freq : subshader.update_frequencies)
        {
            dst->update_frequencies[freq.layout] = freq.frequency;
        }

        // Sampler refs
        for (const auto& ref : subshader.samplers)
        {
            const u32 hash = get_hash(samplers[ref.resource_index]);
            const int existing = find_index_if(dst->samplers, [&](const ShaderSampler& s)
            {
                return hash == s.hash;
            });
            if (existing >= 0)
            {
                continue;
            }

            dst->samplers.resize(dst->samplers.size() + 1);
            auto& sampler = dst->samplers.back();
            sampler.hash = hash;
            sampler.binding = ref.binding;
            sampler.layout = ref.layout;
            memcpy(&sampler.info, &samplers[ref.resource_index].info, sizeof(SamplerCreateInfo));
        }

        ++dst->stages.size;
    }
}


} // namespace bee