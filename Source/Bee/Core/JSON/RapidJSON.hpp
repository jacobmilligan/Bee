/*
 *  RapidJSON.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

/****************************************************************************************************************
 *
 * # RapidJSON.hpp
 *
 * Bee uses the RapidJSON library for most JSON operations. Always use the libraries features by #including
 * this file and #defining the various include macros in your .cpp file rather than including them directly
 * from `<ThirdParty/rapidjson/include>` - this is because the library is header-only and the various configuration
 * options used by Bee overriden using macros here (custom assertion macros, allocation functions etc.)
 *
 * If you're implementing some read-only functionality and would like to use a more relaxed syntax - unquoted
 * keys, no root objects required etc. - for tools in the same vein as Shadecc, use the API defined in
 * `<Bee/Core/JSON/JSON.hpp>` instead.
 *
 * ## Include macros
 *
 * The following macros are defined to allow you to optionally #include parts of the RapidJSON library piecemeal
 * from this header (`<rapidjson/rapidjson.h>` is always included):
 *
 * |            Macro                       |                       Included file                           |
 * |----------------------------------------|---------------------------------------------------------------|
 * |    `BEE_RAPIDJSON_DOCUMENT_H`          |       `<rapidjson/document.h>`                                |
 * |    `BEE_RAPIDJSON_FILEREADSTREAM_H`    |       `<rapidjson/filereadstream.h>`                          |
 * |    `BEE_RAPIDJSON_FILEWRITESTREAM_H`   |       `<rapidjson/filewritestream.h>`                         |
 * |    `BEE_RAPIDJSON_MEMORYBUFFER_H`      |       `<rapidjson/memorybuffer.h>`                            |
 * |    `BEE_RAPIDJSON_MEMORYSTREAM_H`      |       `<rapidjson/memorystream.h>`                            |
 * |    `BEE_RAPIDJSON_READER_H`            |       `<rapidjson/reader.h>`                                  |
 * |    `BEE_RAPIDJSON_STREAM_H`            |       `<rapidjson/stream.h>`                                  |
 * |    `BEE_RAPIDJSON_STRINGBUFFER_H`      |       `<rapidjson/stringbuffer.h>`                            |
 * |    `BEE_RAPIDJSON_WRITER_H`            |       `<rapidjson/writer.h>`                                  |
 * |    `BEE_RAPIDJSON_PRETTYWRITER_H`      |       `<rapidjson/prettywriter.h>`                            |
 * |    `BEE_RAPIDJSON_ERROR_H`             |       `<rapidjson/error/error.h>` &  `<rapidjson/error/en.h>` |
 *
 ****************************************************************************************************************
 */

#pragma once

#include "Bee/Core/Config.hpp"

#undef RAPIDJSON_NEW
#undef RAPIDJSON_DELETE
#undef RAPIDJSON_ASSERT

#define RAPIDJSON_NEW(TypeName) BEE_NEW(bee::system_allocator(), TypeName)

#define RAPIDJSON_DELETE(ptr)                                                   \
    BEE_BEGIN_MACRO_BLOCK                                                       \
        if (ptr != nullptr) { BEE_DELETE(bee::system_allocator(), ptr); }       \
    BEE_END_MACRO_BLOCK

#define RAPIDJSON_ASSERT(x) BEE_ASSERT(x)

BEE_PUSH_WARNING

    BEE_DISABLE_PADDING_WARNINGS
    BEE_DISABLE_WARNING_MSVC(4127)
    #include <rapidjson/fwd.h>

    /*
     * Optional includes
     */
    #ifdef BEE_RAPIDJSON_RAPIDJSON_H
        #include <rapidjson/rapidjson.h>
    #endif // BEE_RAPIDJSON_RAPIDJSON_H

    #ifdef BEE_RAPIDJSON_DOCUMENT_H
        #include <rapidjson/document.h>
    #endif // BEE_RAPIDJSON_DOCUMENT_H

    #ifdef BEE_RAPIDJSON_FILEREADSTREAM_H
        #include <rapidjson/filereadstream.h>
    #endif // BEE_RAPIDJSON_FILEREADSTREAM_H

    #ifdef BEE_RAPIDJSON_FILEWRITESTREAM_H
        #include <rapidjson/filewritestream.h>
    #endif // BEE_RAPIDJSON_FILEWRITESTREAM_H

    #ifdef BEE_RAPIDJSON_MEMORYBUFFER_H
        #include <rapidjson/memorybuffer.h>
    #endif // BEE_RAPIDJSON_MEMORYBUFFER_H

    #ifdef BEE_RAPIDJSON_MEMORYSTREAM_H
        #include <rapidjson/memorystream.h>
    #endif // BEE_RAPIDJSON_MEMORYSTREAM_H

    #ifdef BEE_RAPIDJSON_READER_H
        #include <rapidjson/reader.h>
    #endif // BEE_RAPIDJSON_READER_H

    #ifdef BEE_RAPIDJSON_STREAM_H
        #include <rapidjson/stream.h>
    #endif // BEE_RAPIDJSON_STREAM_H

    #ifdef BEE_RAPIDJSON_STRINGBUFFER_H
        #include <rapidjson/stringbuffer.h>
    #endif // BEE_RAPIDJSON_STRINGBUFFER_H

    #ifdef BEE_RAPIDJSON_WRITER_H
        #include <rapidjson/writer.h>
    #endif // BEE_RAPIDJSON_WRITER_H

    #ifdef BEE_RAPIDJSON_PRETTYWRITER_H
        #include <rapidjson/prettywriter.h>
    #endif // BEE_RAPIDJSON_PRETTYWRITER_H

    #ifdef BEE_RAPIDJSON_ERROR_H
        #include <rapidjson/error/error.h>
        #include <rapidjson/error/en.h>
    #endif // BEE_RAPIDJSON_ERROR_H
BEE_POP_WARNING

