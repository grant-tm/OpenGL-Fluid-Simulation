#include "gui.h"

#include "imgui.h"
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND window_handle, UINT message, WPARAM wide_param, LPARAM long_param);

namespace
{
    struct GuiState
    {
        bool initialized;
        bool show_demo_window;
        bool show_top_bar;
        bool show_simulation_panel;
        bool show_rendering_panel;
        bool show_diagnostics_panel;
    };

    GuiState global_gui_state = {};

    const char *GetRenderModeName(SimulationRenderMode render_mode)
    {
        switch (render_mode)
        {
            case SIMULATION_RENDER_MODE_PARTICLES: return "Particles";
            case SIMULATION_RENDER_MODE_SCREEN_FLUID: return "Screen Fluid";
            default: return "Unknown";
        }
    }

    const char *GetParticleVisualizationModeName(SimulationParticleVisualizationMode particle_visualization_mode)
    {
        switch (particle_visualization_mode)
        {
            case SIMULATION_PARTICLE_VISUALIZATION_BASIC: return "Basic";
            case SIMULATION_PARTICLE_VISUALIZATION_DENSITY: return "Density";
            case SIMULATION_PARTICLE_VISUALIZATION_VELOCITY: return "Velocity";
            case SIMULATION_PARTICLE_VISUALIZATION_SPATIAL_KEY: return "Spatial Key";
            case SIMULATION_PARTICLE_VISUALIZATION_WHITEWATER_WEIGHTED_VELOCITY_DIFFERENCE: return "Whitewater WVD";
            case SIMULATION_PARTICLE_VISUALIZATION_WHITEWATER_TRAPPED_AIR_FACTOR: return "Whitewater Trapped Air";
            case SIMULATION_PARTICLE_VISUALIZATION_WHITEWATER_KINETIC_ENERGY_FACTOR: return "Whitewater Kinetic";
            case SIMULATION_PARTICLE_VISUALIZATION_WHITEWATER_SPAWN_FACTOR: return "Whitewater Spawn";
            default: return "Unknown";
        }
    }

    const char *GetScreenFluidVisualizationModeName(SimulationScreenFluidVisualizationMode screen_fluid_visualization_mode)
    {
        switch (screen_fluid_visualization_mode)
        {
            case SIMULATION_SCREEN_FLUID_VISUALIZATION_COMPOSITE: return "Composite";
            case SIMULATION_SCREEN_FLUID_VISUALIZATION_PACKED: return "Packed";
            case SIMULATION_SCREEN_FLUID_VISUALIZATION_NORMAL: return "Normal";
            case SIMULATION_SCREEN_FLUID_VISUALIZATION_SMOOTH_DEPTH: return "Smooth Depth";
            case SIMULATION_SCREEN_FLUID_VISUALIZATION_HARD_DEPTH: return "Hard Depth";
            case SIMULATION_SCREEN_FLUID_VISUALIZATION_DEPTH_DELTA: return "Depth Delta";
            case SIMULATION_SCREEN_FLUID_VISUALIZATION_FOAM: return "Foam";
            case SIMULATION_SCREEN_FLUID_VISUALIZATION_FOAM_DEPTH: return "Foam Depth";
            case SIMULATION_SCREEN_FLUID_VISUALIZATION_WHITEWATER_SPRAY: return "Spray";
            case SIMULATION_SCREEN_FLUID_VISUALIZATION_WHITEWATER_FOAM: return "Foam";
            case SIMULATION_SCREEN_FLUID_VISUALIZATION_WHITEWATER_BUBBLE: return "Bubble";
            case SIMULATION_SCREEN_FLUID_VISUALIZATION_WHITEWATER_NEIGHBOR_COUNT: return "Neighbors";
            case SIMULATION_SCREEN_FLUID_VISUALIZATION_WHITEWATER_NEWBORN: return "Newborn";
            default: return "Unknown";
        }
    }

    const char *GetScreenFluidSmoothingModeName(SimulationScreenFluidSmoothingMode screen_fluid_smoothing_mode)
    {
        switch (screen_fluid_smoothing_mode)
        {
            case SIMULATION_SCREEN_FLUID_SMOOTHING_BILATERAL: return "Bilateral 1D";
            case SIMULATION_SCREEN_FLUID_SMOOTHING_GAUSSIAN: return "Gaussian";
            case SIMULATION_SCREEN_FLUID_SMOOTHING_BILATERAL_2D: return "Bilateral 2D";
            default: return "Unknown";
        }
    }

    template <typename EnumType>
    bool ComboFromEnumArray(const char *label, EnumType *value, const EnumType *values, const char *const *names, int value_count)
    {
        int current_index = 0;
        for (int value_index = 0; value_index < value_count; ++value_index)
        {
            if (*value == values[value_index])
            {
                current_index = value_index;
                break;
            }
        }

        bool changed = ImGui::Combo(label, &current_index, names, value_count);
        if (changed)
        {
            *value = values[current_index];
        }
        return changed;
    }

    void DrawTopBar(GuiFrameData *frame_data)
    {
        ImGuiViewport *main_viewport = ImGui::GetMainViewport();
        const ImGuiWindowFlags window_flags =
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoBringToFrontOnFocus;

        ImGui::SetNextWindowPos(main_viewport->WorkPos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(main_viewport->WorkSize.x, 54.0f), ImGuiCond_Always);

        if (ImGui::Begin("Top Bar", nullptr, window_flags))
        {
            const char *pause_label = *frame_data->simulation_is_paused ? "Resume" : "Pause";
            if (ImGui::Button(pause_label))
            {
                *frame_data->simulation_is_paused = !*frame_data->simulation_is_paused;
            }
            ImGui::SameLine();
            if (ImGui::Button("Step"))
            {
                *frame_data->simulation_single_step_requested = true;
                *frame_data->simulation_is_paused = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Reset"))
            {
                *frame_data->reset_requested = true;
            }

            ImGui::SameLine(0.0f, 24.0f);
            ImGui::Text("Mode: %s", GetRenderModeName(*frame_data->render_mode));
            ImGui::SameLine(0.0f, 18.0f);
            if (*frame_data->render_mode == SIMULATION_RENDER_MODE_PARTICLES)
            {
                ImGui::Text("View: %s", GetParticleVisualizationModeName(*frame_data->particle_visualization_mode));
            }
            else
            {
                ImGui::Text(
                    "View: %s | Smooth: %s",
                    GetScreenFluidVisualizationModeName(*frame_data->screen_fluid_visualization_mode),
                    GetScreenFluidSmoothingModeName(*frame_data->screen_fluid_smoothing_mode));
            }

            ImGui::Text(
                "FPS %.0f | Frame %.2f ms | Sim %.2f ms | Render %.2f ms | GPU Blur %.2f ms | WW %u",
                frame_data->current_frames_per_second,
                frame_data->current_frame_delta_time_milliseconds,
                (f32) frame_data->simulation_timings.total_milliseconds,
                (f32) frame_data->renderer_timings.total_milliseconds,
                (f32) frame_data->renderer_timings.gpu_blur_milliseconds,
                frame_data->whitewater_active_count);
        }
        ImGui::End();
    }

    void DrawSimulationPanel(GuiFrameData *frame_data)
    {
        ImGuiViewport *main_viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2(main_viewport->WorkPos.x + 12.0f, main_viewport->WorkPos.y + 66.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(360.0f, 560.0f), ImGuiCond_FirstUseEver);

        if (!ImGui::Begin("Simulation", &global_gui_state.show_simulation_panel))
        {
            ImGui::End();
            return;
        }

        if (ImGui::CollapsingHeader("Time", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::DragFloat("Time scale", &frame_data->step_settings->time_scale, 0.01f, 0.0f, 4.0f, "%.2f");
            ImGui::DragFloat("Max delta time", &frame_data->step_settings->maximum_delta_time, 0.0005f, 0.001f, 0.100f, "%.4f");
            ImGui::DragInt("Iterations / frame", &frame_data->step_settings->iterations_per_frame, 1.0f, 1, 16);
        }

        if (ImGui::CollapsingHeader("Fluid", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::DragFloat("Gravity", &frame_data->step_settings->gravity, 0.1f, -50.0f, 0.0f, "%.2f");
            ImGui::DragFloat("Pressure multiplier", &frame_data->pressure_settings->pressure_multiplier, 0.5f, 0.0f, 1000.0f, "%.2f");
            ImGui::DragFloat("Near pressure", &frame_data->pressure_settings->near_pressure_multiplier, 0.05f, 0.0f, 100.0f, "%.2f");
            ImGui::DragFloat("Target density", &frame_data->pressure_settings->target_density, 1.0f, 0.0f, 2000.0f, "%.2f");
            ImGui::DragFloat("Viscosity", &frame_data->viscosity_settings->viscosity_strength, 0.005f, 0.0f, 1.0f, "%.3f");
            ImGui::DragFloat("Smoothing radius", &frame_data->pressure_settings->smoothing_radius, 0.005f, 0.01f, 2.0f, "%.3f");
            frame_data->viscosity_settings->smoothing_radius = frame_data->pressure_settings->smoothing_radius;
        }

        if (ImGui::CollapsingHeader("Collision", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::DragFloat("Collision damping", &frame_data->collision_settings->collision_damping, 0.01f, 0.0f, 1.0f, "%.2f");
            ImGui::DragFloat("Min bounce speed", &frame_data->collision_settings->minimum_bounce_speed, 0.05f, 0.0f, 10.0f, "%.2f");
        }

        if (ImGui::CollapsingHeader("Whitewater", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::DragFloat("Spawn rate", &frame_data->whitewater_settings->spawn_rate, 1.0f, 0.0f, 2000.0f, "%.1f");
            ImGui::DragFloat("Spawn fade start", &frame_data->whitewater_settings->spawn_rate_fade_start_time, 0.01f, 0.0f, 10.0f, "%.2f");
            ImGui::DragFloat("Spawn fade in", &frame_data->whitewater_settings->spawn_rate_fade_in_time, 0.01f, 0.0f, 10.0f, "%.2f");
            ImGui::DragFloat("Trapped air min", &frame_data->whitewater_settings->trapped_air_velocity_minimum, 0.1f, 0.0f, 100.0f, "%.2f");
            ImGui::DragFloat("Trapped air max", &frame_data->whitewater_settings->trapped_air_velocity_maximum, 0.1f, 0.0f, 100.0f, "%.2f");
            ImGui::DragFloat("Kinetic min", &frame_data->whitewater_settings->kinetic_energy_minimum, 0.1f, 0.0f, 100.0f, "%.2f");
            ImGui::DragFloat("Kinetic max", &frame_data->whitewater_settings->kinetic_energy_maximum, 0.1f, 0.0f, 100.0f, "%.2f");
            ImGui::DragInt("Surface max neighbors", &frame_data->whitewater_settings->surface_spawn_maximum_neighbors, 1.0f, 0, 64);
            ImGui::DragInt("Spray max neighbors", &frame_data->whitewater_settings->spray_classify_maximum_neighbors, 1.0f, 0, 64);
            ImGui::DragInt("Bubble min neighbors", &frame_data->whitewater_settings->bubble_classify_minimum_neighbors, 1.0f, 0, 64);
            ImGui::DragFloat("Bubble buoyancy", &frame_data->whitewater_settings->bubble_buoyancy, 0.05f, 0.0f, 5.0f, "%.2f");
            ImGui::DragFloat("Bubble scale", &frame_data->whitewater_settings->bubble_scale, 0.01f, 0.0f, 2.0f, "%.2f");
            ImGui::DragFloat("Scale change speed", &frame_data->whitewater_settings->bubble_scale_change_speed, 0.1f, 0.0f, 20.0f, "%.2f");
            ImGui::DragFloat("WW collision damping", &frame_data->whitewater_settings->collision_damping, 0.01f, 0.0f, 1.0f, "%.2f");
        }

        ImGui::End();
    }

    void DrawRenderingPanel(GuiFrameData *frame_data)
    {
        static const SimulationRenderMode render_modes[] =
        {
            SIMULATION_RENDER_MODE_PARTICLES,
            SIMULATION_RENDER_MODE_SCREEN_FLUID,
        };
        static const char *const render_mode_names[] =
        {
            "Particles",
            "Screen Fluid",
        };
        static const SimulationParticleVisualizationMode particle_visualization_modes[] =
        {
            SIMULATION_PARTICLE_VISUALIZATION_BASIC,
            SIMULATION_PARTICLE_VISUALIZATION_DENSITY,
            SIMULATION_PARTICLE_VISUALIZATION_VELOCITY,
            SIMULATION_PARTICLE_VISUALIZATION_SPATIAL_KEY,
            SIMULATION_PARTICLE_VISUALIZATION_WHITEWATER_WEIGHTED_VELOCITY_DIFFERENCE,
            SIMULATION_PARTICLE_VISUALIZATION_WHITEWATER_TRAPPED_AIR_FACTOR,
            SIMULATION_PARTICLE_VISUALIZATION_WHITEWATER_KINETIC_ENERGY_FACTOR,
            SIMULATION_PARTICLE_VISUALIZATION_WHITEWATER_SPAWN_FACTOR,
        };
        static const char *const particle_visualization_names[] =
        {
            "Basic",
            "Density",
            "Velocity",
            "Spatial Key",
            "Whitewater WVD",
            "Whitewater Trapped Air",
            "Whitewater Kinetic",
            "Whitewater Spawn",
        };
        static const SimulationScreenFluidVisualizationMode screen_fluid_visualization_modes[] =
        {
            SIMULATION_SCREEN_FLUID_VISUALIZATION_COMPOSITE,
            SIMULATION_SCREEN_FLUID_VISUALIZATION_PACKED,
            SIMULATION_SCREEN_FLUID_VISUALIZATION_NORMAL,
            SIMULATION_SCREEN_FLUID_VISUALIZATION_SMOOTH_DEPTH,
            SIMULATION_SCREEN_FLUID_VISUALIZATION_HARD_DEPTH,
            SIMULATION_SCREEN_FLUID_VISUALIZATION_DEPTH_DELTA,
            SIMULATION_SCREEN_FLUID_VISUALIZATION_FOAM,
            SIMULATION_SCREEN_FLUID_VISUALIZATION_FOAM_DEPTH,
            SIMULATION_SCREEN_FLUID_VISUALIZATION_WHITEWATER_SPRAY,
            SIMULATION_SCREEN_FLUID_VISUALIZATION_WHITEWATER_FOAM,
            SIMULATION_SCREEN_FLUID_VISUALIZATION_WHITEWATER_BUBBLE,
            SIMULATION_SCREEN_FLUID_VISUALIZATION_WHITEWATER_NEIGHBOR_COUNT,
            SIMULATION_SCREEN_FLUID_VISUALIZATION_WHITEWATER_NEWBORN,
        };
        static const char *const screen_fluid_visualization_names[] =
        {
            "Composite",
            "Packed",
            "Normal",
            "Smooth Depth",
            "Hard Depth",
            "Depth Delta",
            "Foam",
            "Foam Depth",
            "Spray",
            "Foam",
            "Bubble",
            "Neighbors",
            "Newborn",
        };
        static const SimulationScreenFluidSmoothingMode screen_fluid_smoothing_modes[] =
        {
            SIMULATION_SCREEN_FLUID_SMOOTHING_BILATERAL,
            SIMULATION_SCREEN_FLUID_SMOOTHING_GAUSSIAN,
            SIMULATION_SCREEN_FLUID_SMOOTHING_BILATERAL_2D,
        };
        static const char *const screen_fluid_smoothing_names[] =
        {
            "Bilateral 1D",
            "Gaussian",
            "Bilateral 2D",
        };

        ImGuiViewport *main_viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(
            ImVec2(main_viewport->WorkPos.x + main_viewport->WorkSize.x - 372.0f, main_viewport->WorkPos.y + 66.0f),
            ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(360.0f, 560.0f), ImGuiCond_FirstUseEver);

        if (!ImGui::Begin("Rendering", &global_gui_state.show_rendering_panel))
        {
            ImGui::End();
            return;
        }

        if (ImGui::CollapsingHeader("Mode", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ComboFromEnumArray("Render mode", frame_data->render_mode, render_modes, render_mode_names, IM_ARRAYSIZE(render_modes));

            if (*frame_data->render_mode == SIMULATION_RENDER_MODE_PARTICLES)
            {
                ComboFromEnumArray(
                    "Particle view",
                    frame_data->particle_visualization_mode,
                    particle_visualization_modes,
                    particle_visualization_names,
                    IM_ARRAYSIZE(particle_visualization_modes));
            }
            else
            {
                ComboFromEnumArray(
                    "Fluid view",
                    frame_data->screen_fluid_visualization_mode,
                    screen_fluid_visualization_modes,
                    screen_fluid_visualization_names,
                    IM_ARRAYSIZE(screen_fluid_visualization_modes));
                ComboFromEnumArray(
                    "Smoothing",
                    frame_data->screen_fluid_smoothing_mode,
                    screen_fluid_smoothing_modes,
                    screen_fluid_smoothing_names,
                    IM_ARRAYSIZE(screen_fluid_smoothing_modes));
            }
        }

        if (ImGui::CollapsingHeader("Screen Fluid", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::DragFloat("Thickness radius", &frame_data->scale_model->screen_fluid_thickness_particle_radius, 0.001f, 0.001f, 2.0f, "%.3f");
            ImGui::DragFloat("Depth radius", &frame_data->scale_model->screen_fluid_depth_particle_radius, 0.001f, 0.001f, 2.0f, "%.3f");
            ImGui::DragFloat("Blur radius", &frame_data->scale_model->screen_fluid_blur_world_radius, 0.001f, 0.001f, 2.0f, "%.3f");
            ImGui::DragFloat("Foam billboard scale", &frame_data->scale_model->whitewater_billboard_scale, 0.001f, 0.001f, 2.0f, "%.3f");
        }

        if (ImGui::CollapsingHeader("Scale", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Text("Particle spacing: %.4f", frame_data->scale_model->particle_spacing);
            ImGui::Text("Smoothing radius: %.4f", frame_data->scale_model->smoothing_radius);
            ImGui::Text("Main particles: %u", frame_data->main_particle_count);
            ImGui::Text("Whitewater active: %u", frame_data->whitewater_active_count);
        }

        ImGui::End();
    }

    void DrawDiagnosticsPanel(const GuiFrameData *frame_data)
    {
        ImGuiViewport *main_viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(
            ImVec2(main_viewport->WorkPos.x + 12.0f, main_viewport->WorkPos.y + main_viewport->WorkSize.y - 258.0f),
            ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(main_viewport->WorkSize.x - 24.0f, 246.0f), ImGuiCond_FirstUseEver);

        if (!ImGui::Begin("Diagnostics", &global_gui_state.show_diagnostics_panel))
        {
            ImGui::End();
            return;
        }

        if (ImGui::BeginTable("timings", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("Metric");
            ImGui::TableSetupColumn("Simulation (ms)");
            ImGui::TableSetupColumn("Renderer CPU (ms)");
            ImGui::TableSetupColumn("Renderer GPU (ms)");
            ImGui::TableHeadersRow();

            auto add_row = [&](const char *label, f64 simulation_value, f64 renderer_cpu_value, f64 renderer_gpu_value)
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(label);
                ImGui::TableNextColumn();
                ImGui::Text("%.2f", simulation_value);
                ImGui::TableNextColumn();
                ImGui::Text("%.2f", renderer_cpu_value);
                ImGui::TableNextColumn();
                ImGui::Text("%.2f", renderer_gpu_value);
            };

            add_row("Total", frame_data->simulation_timings.total_milliseconds, frame_data->renderer_timings.total_milliseconds, frame_data->renderer_timings.gpu_total_milliseconds);
            add_row("Hash", frame_data->simulation_timings.spatial_hash_milliseconds, 0.0, 0.0);
            add_row("Density", frame_data->simulation_timings.density_milliseconds, 0.0, 0.0);
            add_row("Pressure", frame_data->simulation_timings.pressure_milliseconds, 0.0, 0.0);
            add_row("Viscosity", frame_data->simulation_timings.viscosity_milliseconds, 0.0, 0.0);
            add_row("Collision", frame_data->simulation_timings.collision_milliseconds, 0.0, 0.0);
            add_row("Blur", 0.0, frame_data->renderer_timings.blur_milliseconds, frame_data->renderer_timings.gpu_blur_milliseconds);
            add_row("Composite", 0.0, frame_data->renderer_timings.composite_milliseconds, frame_data->renderer_timings.gpu_composite_milliseconds);
            add_row("Whitewater", 0.0, frame_data->renderer_timings.whitewater_milliseconds, 0.0);

            ImGui::EndTable();
        }

        ImGui::Separator();
        ImGui::Text(
            "Frame %.2f ms | Swap %.2f ms | FPS %.0f | Main %u | Whitewater %u",
            frame_data->current_frame_delta_time_milliseconds,
            frame_data->current_swap_buffers_milliseconds,
            frame_data->current_frames_per_second,
            frame_data->main_particle_count,
            frame_data->whitewater_active_count);

        ImGui::End();
    }
}

bool Gui_Initialize(HWND window_handle)
{
    if (global_gui_state.initialized)
    {
        return true;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    if (!ImGui_ImplWin32_InitForOpenGL(window_handle))
    {
        ImGui::DestroyContext();
        return false;
    }

    if (!ImGui_ImplOpenGL3_Init("#version 330"))
    {
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        return false;
    }

    global_gui_state.initialized = true;
    global_gui_state.show_demo_window = false;
    global_gui_state.show_top_bar = true;
    global_gui_state.show_simulation_panel = true;
    global_gui_state.show_rendering_panel = true;
    global_gui_state.show_diagnostics_panel = true;
    return true;
}

void Gui_Shutdown(void)
{
    if (!global_gui_state.initialized)
    {
        return;
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    global_gui_state = {};
}

void Gui_BeginFrame(void)
{
    if (!global_gui_state.initialized)
    {
        return;
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void Gui_Draw(GuiFrameData *frame_data)
{
    if (!global_gui_state.initialized)
    {
        return;
    }

    if (frame_data != nullptr)
    {
        if (global_gui_state.show_top_bar)
        {
            DrawTopBar(frame_data);
        }
        if (global_gui_state.show_simulation_panel)
        {
            DrawSimulationPanel(frame_data);
        }
        if (global_gui_state.show_rendering_panel)
        {
            DrawRenderingPanel(frame_data);
        }
        if (global_gui_state.show_diagnostics_panel)
        {
            DrawDiagnosticsPanel(frame_data);
        }
    }

    if (global_gui_state.show_demo_window)
    {
        ImGui::ShowDemoWindow(&global_gui_state.show_demo_window);
    }
}

void Gui_Render(void)
{
    if (!global_gui_state.initialized)
    {
        return;
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

bool Gui_HandleWindowMessage(HWND window_handle, UINT message, WPARAM wide_param, LPARAM long_param)
{
    if (!global_gui_state.initialized)
    {
        return false;
    }

    return ImGui_ImplWin32_WndProcHandler(window_handle, message, wide_param, long_param) != 0;
}

bool Gui_WantsCaptureKeyboard(void)
{
    if (!global_gui_state.initialized)
    {
        return false;
    }

    return ImGui::GetIO().WantCaptureKeyboard;
}

bool Gui_WantsCaptureMouse(void)
{
    if (!global_gui_state.initialized)
    {
        return false;
    }

    return ImGui::GetIO().WantCaptureMouse;
}
