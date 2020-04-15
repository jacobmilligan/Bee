/*
 *  Shader.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/Graphics/Shader.hpp"

namespace bee {


//void* ShaderLoader::allocate(const Type* type)
//{
//    if (type != get_type<Shader>())
//    {
//        return nullptr;
//    }
//
//    scoped_recursive_spinlock_t lock(allocator_lock_);
//    return BEE_NEW(allocator_, Shader);
//}
//
//AssetStatus ShaderLoader::load(AssetLoadContext* ctx, io::Stream* src_stream, const DeviceHandle& device)
//{
//    BEE_ASSERT(ctx->type() == get_type<Shader>());
//
//    auto shader = ctx->asset_ptr<Shader>();
//    StreamSerializer serializer(src_stream);
//    serialize(SerializerMode::reading, &serializer, shader);
//
//    // keep a handle to the gpu device so resources can be unloaded later
//    shader->gpu_device = device;
//
//    // initialize the render passes
//    RenderPassCreateInfo pass_info{};
//    for (auto& pass : shader->passes)
//    {
//        pass_info.subpass_count = pass.subpasses.size;
//        pass_info.subpasses = shader->subpasses.data() + pass.subpasses.offset;
//        pass_info.attachment_count = pass.attachments.size;
//        memcpy(pass_info.attachments, shader->attachments.data() + pass.attachments.offset, pass_info.attachment_count * sizeof(AttachmentDescriptor));
//        pass.gpu_handle = gpu_create_render_pass(device, pass_info);
//
//        BEE_ASSERT(pass.gpu_handle.is_valid());
//    }
//
//    // initialize the GPU shaders
//    ShaderCreateInfo shader_info{};
//    for (auto& subshader : shader->subshaders)
//    {
//        for (int stage = 0; stage < static_array_length(subshader.stage_entries); ++stage)
//        {
//            auto& code_range = subshader.stage_code_ranges[stage];
//            if (code_range.size < 0 || code_range.offset < 0)
//            {
//                continue;
//            }
//
//            shader_info.entry = subshader.stage_entries[stage].c_str();
//            shader_info.code_size = static_cast<size_t>(code_range.size);
//            shader_info.code = shader->code.data() + code_range.offset;
//
//            subshader.stage_handles[stage] = gpu_create_shader(device, shader_info);
//        }
//    }
//
//    // Now that we have valid render passes and shaders - initialize all the pipelines
//    for (auto& pipeline : shader->pipelines)
//    {
//        pipeline.info.compatible_render_pass = shader->passes[pipeline.pass].gpu_handle;
//
//        auto& vertex_shader = shader->subshaders[pipeline.shaders[underlying_t(ShaderStageIndex::vertex)]];
//        auto& fragment_shader = shader->subshaders[pipeline.shaders[underlying_t(ShaderStageIndex::fragment)]];
//
//        pipeline.info.vertex_stage = vertex_shader.stage_handles[underlying_t(ShaderStageIndex::vertex)];
//        pipeline.info.fragment_stage = fragment_shader.stage_handles[underlying_t(ShaderStageIndex::fragment)];
//
//        pipeline.gpu_handle = gpu_create_pipeline_state(device, pipeline.info);
//    }
//
//    return AssetStatus::loaded;
//}
//
//AssetStatus ShaderLoader::unload(AssetLoadContext* ctx, const AssetUnloadKind unload_type)
//{
//    scoped_recursive_spinlock_t lock(allocator_lock_);
//    BEE_DELETE(allocator_, ctx->ptr());
//    return AssetStatus::unloaded;
//}


} // namespace bee