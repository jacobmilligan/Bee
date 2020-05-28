/*
 *  Win32GUID.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/GUID.hpp"

#define BEE_MINWINDOWS_ENABLE_MSG
#define BEE_MINWINDOWS_ENABLE_USER
#include "Bee/Core/Win32/MinWindows.h"

#include <combaseapi.h>

namespace bee {


GUID generate_guid()
{
    ::GUID win_guid{};
    CoCreateGuid(&win_guid);

    OLECHAR guid_str[64]{};
    StringFromGUID2(win_guid, guid_str, 64);

    /*
     * Given a GUID in the form: AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE:
     */
    GUID guid{};
    // copy AAAAAAAA
    memcpy(guid.data, &win_guid.Data1, sizeof(win_guid.Data1));
    // copy BBBB
    memcpy(guid.data + 4, &win_guid.Data2, sizeof(win_guid.Data2));
    // copy CCCC
    memcpy(guid.data + 6, &win_guid.Data3, sizeof(win_guid.Data3));
    // copy DDDD
    guid.data[8] = win_guid.Data4[1];
    guid.data[9] = win_guid.Data4[0];
    // copy EEEEEEEEEEEE
    guid.data[10] = win_guid.Data4[7];
    guid.data[11] = win_guid.Data4[6];
    guid.data[12] = win_guid.Data4[5];
    guid.data[13] = win_guid.Data4[4];
    guid.data[14] = win_guid.Data4[3];
    guid.data[15] = win_guid.Data4[2];

    return guid;
}


} // namespace bee
