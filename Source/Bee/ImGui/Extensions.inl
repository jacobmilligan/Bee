/*
 *  Extensions.inl
 *  Bee
 *
 *  Copyright (c) 2021 Jacob Milligan. All rights reserved.
 */

#pragma once

bool (*InputTextLeft)(const char* label, char* buf, const size_t buf_size, const ImGuiInputTextFlags flags, const ImGuiInputTextCallback callback, void* user_data) { nullptr };
