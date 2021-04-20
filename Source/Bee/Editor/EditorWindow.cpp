/*
 *  EditorWindow.cpp
 *  Bee
 *
 *  Copyright (c) 2021 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Path.hpp"

#include "Bee/Editor/EditorWindow.inl"
#include "Bee/ImGui/ImGui.hpp"

namespace bee {


void tick_new_project_window(PlatformModule* platform, ImGuiModule* imgui, NewProjectWindow* window)
{
    imgui->SetNextWindowSize(ImVec2 { 450, 100 }, 0);
    if (!imgui->Begin("New project", &window->open, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
    {
        return;
    }

    if (window->open)
    {
        imgui->InputTextLeft("Name    ", window->name.data(), window->name.capacity(), 0, nullptr, nullptr);
        imgui->InputTextLeft("Location", window->location.data(), window->location.capacity(), 0, nullptr, nullptr);
        imgui->SameLine(0.0f, -1.0f);
        if (imgui->Button("Browse...", ImVec2{ 0, 0 }))
        {
            Path path;
            if (platform->open_file_dialog(&path))
            {
                log_info("%s", path.c_str());
            }
        }

        if (imgui->Button("Okay", ImVec2{}))
        {
            window->open = false;
        }
        imgui->SameLine(0.0f, -1.0f);
        if (imgui->Button("Cancel", ImVec2{}))
        {
            window->open = false;
        }
    }

    imgui->End();
//    auto res = g_project->open()
}

void tick_editor_window(PlatformModule* platform, ImGuiModule* imgui, EditorWindow* window)
{
    if (imgui->BeginMainMenuBar())
    {
        if (imgui->BeginMenu("File", true))
        {
            if (imgui->MenuItemBool("New project...", nullptr, false, true))
            {
                window->new_project.open = true;
            }
            imgui->EndMenu();
        }
        imgui->EndMainMenuBar();
    }

    if (window->new_project.open)
    {
        tick_new_project_window(platform, imgui, &window->new_project);
    }
}


} // namespace bee