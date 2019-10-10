/*
 *  GPULimits.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once


/*
 ********************************************************
 *
 * # GPU Device limits
 *
 * Limits used for storing GPU resources and objects.
 * Can be overriden as a macro in user code
 *
 ********************************************************
 */

#ifndef BEE_GPU_MAX_FRAMES_IN_FLIGHT
    #define BEE_GPU_MAX_FRAMES_IN_FLIGHT 3u
#endif // BEE_GPU_MAX_FRAMES_IN_FLIGHT

#ifndef BEE_GPU_MAX_PHYSICAL_DEVICES
    #define BEE_GPU_MAX_PHYSICAL_DEVICES 4u
#endif // BEE_GPU_MAX_PHYSICAL_DEVICES

#ifndef BEE_GPU_MAX_DEVICES
    #define BEE_GPU_MAX_DEVICES 1u
#endif // BEE_GPU_MAX_DEVICES

#ifndef BEE_GPU_MAX_SWAPCHAINS
    #define BEE_GPU_MAX_SWAPCHAINS 4u
#endif // BEE_GPU_MAX_SWAPCHAINS

#ifndef BEE_GPU_MAX_RENDER_PASSES
    #define BEE_GPU_MAX_RENDER_PASSES 128u
#endif // BEE_GPU_MAX_RENDER_PASSES

#ifndef BEE_GPU_MAX_RESOURCE_POOLS
    #define BEE_GPU_MAX_RESOURCE_POOLS 16u
#endif // BEE_GPU_MAX_RESOURCE_POOLS

// Maximum one unique framebuffer per unique render pass
#ifndef BEE_GPU_MAX_FRAMEBUFFERS
    #define BEE_GPU_MAX_FRAMEBUFFERS BEE_GPU_MAX_RENDER_PASSES
#endif // BEE_GPU_MAX_FRAMEBUFFERS

#ifndef BEE_GPU_MAX_FENCES
    #define BEE_GPU_MAX_FENCES 16u
#endif // BEE_GPU_MAX_FENCES

#ifndef BEE_GPU_MAX_COMMAND_BUFFERS
    #define BEE_GPU_MAX_COMMAND_BUFFERS 16u
#endif // BEE_GPU_MAX_COMMAND_BUFFERS

#ifndef BEE_GPU_MAX_PIPELINE_STATES
    #define BEE_GPU_MAX_PIPELINE_STATES 128u
#endif // BEE_GPU_MAX_PIPELINE_STATES

#ifndef BEE_GPU_MAX_DEPTH_STENCIL_STATES
    #define BEE_GPU_MAX_DEPTH_STENCIL_STATES 8u
#endif // BEE_GPU_MAX_DEPTH_STENCIL_STATES

#ifndef BEE_GPU_MAX_SHADERS
    #define BEE_GPU_MAX_SHADERS 128u
#endif // BEE_GPU_MAX_SHADERS

#ifndef BEE_GPU_MAX_TEXTURES
    #define BEE_GPU_MAX_TEXTURES 512u
#endif // BEE_GPU_MAX_TEXTURES

#ifndef BEE_GPU_MAX_TEXTURE_VIEWS
    #define BEE_GPU_MAX_TEXTURE_VIEWS BEE_GPU_MAX_TEXTURES
#endif // BEE_GPU_MAX_TEXTURE_VIEWS

#ifndef BEE_GPU_MAX_BUFFERS
    #define BEE_GPU_MAX_BUFFERS 512u
#endif // BEE_GPU_MAX_BUFFERS

#ifndef BEE_GPU_MAX_SAMPLERS
    #define BEE_GPU_MAX_SAMPLERS 16u
#endif // BEE_GPU_MAX_SAMPLERS

// based off the metal feature set - seems to be the lowest of the API's
// see: https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf
#define BEE_GPU_MAX_ATTACHMENTS 8u

#ifndef BEE_GPU_MAX_COMMAND_POOLS
    #define BEE_GPU_MAX_COMMAND_POOLS 16u
#endif // BEE_GPU_MAX_COMMAND_POOLS