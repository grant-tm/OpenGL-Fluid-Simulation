#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "base.h"
#include "glad.h"
#include "opengl_helpers.h"
#include "simulation_data.h"
#include "simulation_collision.h"
#include "simulation_density.h"
#include "simulation_pipeline.h"
#include "simulation_pressure.h"
#include "simulation_renderer.h"
#include "simulation_reorder.h"
#include "simulation_spawner.h"
#include "simulation_spatial_hash.h"
#include "simulation_step.h"
#include "simulation_viscosity.h"
#include "simulation_volume_density.h"
#include "simulation_whitewater.h"

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

typedef struct Application
{
    HINSTANCE instance_handle;
    HWND window_handle;
    HDC device_context_handle;
    HGLRC rendering_context_handle;
    bool is_running;
    bool was_resized;
    i32 client_width;
    i32 client_height;
    SimulationParticleBuffers particle_buffers;
    SimulationRenderer renderer;
    SimulationCamera camera;
    SimulationPipeline pipeline;
    SimulationPipelineSettings pipeline_settings;
    SimulationStepSettings step_settings;
    SimulationStepper stepper;
    SimulationCollision collision;
    SimulationCollisionSettings collision_settings;
    SimulationDensity density;
    SimulationDensitySettings density_settings;
    SimulationPressure pressure;
    SimulationPressureSettings pressure_settings;
    SimulationViscosity viscosity;
    SimulationViscositySettings viscosity_settings;
    SimulationSpatialHash spatial_hash;
    SimulationSpatialHashSettings spatial_hash_settings;
    SimulationVolumeDensity volume_density;
    SimulationVolumeDensitySettings volume_density_settings;
    SimulationWhitewater whitewater;
    SimulationWhitewaterSettings whitewater_settings;
    Vec3 simulation_bounds_size;
    SimulationSpawnData initial_spawn_data;
    bool reset_requested;
    bool simulation_is_paused;
    bool simulation_single_step_requested;
    f32 simulation_accumulator_seconds;
    f32 fixed_simulation_delta_time_seconds;
    f32 maximum_frame_delta_time_seconds;
    f32 velocity_visualization_minimum;
    f32 velocity_visualization_maximum;
    f32 window_title_update_accumulator_seconds;
    f32 most_recent_frame_delta_time_seconds;
    f32 most_recent_swap_buffers_milliseconds;
    u32 most_recent_whitewater_particle_count;
    SimulationRenderMode render_mode;
    SimulationParticleVisualizationMode particle_visualization_mode;
    SimulationScreenFluidVisualizationMode screen_fluid_visualization_mode;
    SimulationScreenFluidSmoothingMode screen_fluid_smoothing_mode;
    f32 density_visualization_minimum;
    f32 density_visualization_maximum;
    f32 simulation_time_seconds;
} Application;

// == Constants =======================================================================================================

#define WINDOW_CLASS_NAME "FluidSimWindowClass"
#define WINDOW_TITLE "FluidSim"

// == Forward Declarations ============================================================================================

static LRESULT CALLBACK MainWindowProc(HWND window_handle, UINT message, WPARAM wide_param, LPARAM long_param);
static bool Win32_RegisterWindowClass(HINSTANCE instance_handle);
static bool Win32_CreateWindowAndContext(Application *application, i32 client_width, i32 client_height);
static void Win32_DestroyWindowAndContext(Application *application);
static bool Application_InitializeSimulationView(Application *application);
static void Application_ShutdownSimulationResources(Application *application);
static bool Application_InitializeSimulationResources(Application *application);
static bool Application_RunInitializationWarmup(Application *application);
static bool Application_RebuildSimulationDerivedState(Application *application);
static bool Application_RunSimulationFixedStep(Application *application, f32 simulation_delta_time_seconds);
static void Application_ResetSimulation(Application *application);
static void Application_UpdateSimulation(Application *application, f32 frame_delta_time_seconds);
static void Application_UpdateDensityVisualizationRange(Application *application);
static void Application_UpdateVelocityVisualizationRange(Application *application);
static void Application_LogSpatialHashInspection(Application *application);
static void Application_LogVolumeDensitySummary(Application *application);
static const char *Application_GetRenderModeName(SimulationRenderMode render_mode);
static const char *Application_GetVisualizationModeName(SimulationParticleVisualizationMode particle_visualization_mode);
static const char *Application_GetScreenFluidVisualizationModeName(SimulationScreenFluidVisualizationMode screen_fluid_visualization_mode);
static const char *Application_GetScreenFluidSmoothingModeName(SimulationScreenFluidSmoothingMode screen_fluid_smoothing_mode);
static void OpenGL_LogContextInfo(void);
static void OpenGL_UpdateViewport(Application *application);
static void OpenGL_RenderFrame(Application *application, f32 delta_time_seconds);
static void Application_ProcessMessages(Application *application);
static void Application_Run(Application *application);
static void Application_UpdateWindowTitle(Application *application);

// == Entry Point =====================================================================================================

int main(void)
{
    Application application = {0};
    application.instance_handle = GetModuleHandleA(NULL);
    application.client_width = 1280;
    application.client_height = 720;

    if (!Win32_RegisterWindowClass(application.instance_handle))
    {
        Base_LogError("Failed to register the main window class.");
        return 1;
    }

    if (!Win32_CreateWindowAndContext(&application, application.client_width, application.client_height))
    {
        Base_LogError("Failed to create the window or OpenGL context.");
        return 1;
    }

    if (!Base_RunSelfChecks())
    {
        return 1;
    }

    if (!OpenGL_RunComputeValidation())
    {
        return 1;
    }

    if (!Simulation_RunDataModelValidation())
    {
        return 1;
    }

    if (!Simulation_RunSpawnerValidation())
    {
        return 1;
    }

    if (!SimulationSpatialHash_RunValidation())
    {
        return 1;
    }

    if (!SimulationReorder_RunValidation())
    {
        return 1;
    }

    if (!SimulationDensity_RunValidation())
    {
        return 1;
    }

    if (!SimulationPressure_RunValidation())
    {
        return 1;
    }

    if (!SimulationViscosity_RunValidation())
    {
        return 1;
    }

    if (!SimulationCollision_RunValidation())
    {
        return 1;
    }

    if (!SimulationPipeline_RunValidation())
    {
        return 1;
    }

    if (!SimulationVolumeDensity_RunValidation())
    {
        return 1;
    }

    if (!SimulationWhitewater_RunValidation())
    {
        return 1;
    }

    if (!Application_InitializeSimulationView(&application))
    {
        Base_LogError("Failed to initialize the baseline simulation renderer.");
        return 1;
    }

    OpenGL_LogContextInfo();
    Application_UpdateWindowTitle(&application);
    Application_Run(&application);
    Win32_DestroyWindowAndContext(&application);

    return 0;
}

// == Windowing =======================================================================================================

static LRESULT CALLBACK MainWindowProc(HWND window_handle, UINT message, WPARAM wide_param, LPARAM long_param)
{
    Application *application = (Application *) GetWindowLongPtrA(window_handle, GWLP_USERDATA);

    if (message == WM_NCCREATE)
    {
        CREATESTRUCTA *create_struct = (CREATESTRUCTA *) long_param;
        application = (Application *) create_struct->lpCreateParams;
        SetWindowLongPtrA(window_handle, GWLP_USERDATA, (LONG_PTR) application);
        return TRUE;
    }

    switch (message)
    {
        case WM_CLOSE:
        {
            if (application != NULL) application->is_running = false;
            DestroyWindow(window_handle);
            return 0;
        }

        case WM_DESTROY:
        {
            PostQuitMessage(0);
            return 0;
        }

        case WM_SIZE:
        {
            if (application != NULL)
            {
                application->client_width = LOWORD(long_param);
                application->client_height = HIWORD(long_param);
                application->was_resized = true;
                Base_LogInfo("Resize: %d x %d", application->client_width, application->client_height);
            }
            return 0;
        }

        case WM_KEYDOWN:
        {
            if (wide_param == VK_ESCAPE)
            {
                if (application != NULL) application->is_running = false;
                DestroyWindow(window_handle);
                return 0;
            }
            if (wide_param == 'R')
            {
                if (application != NULL) application->reset_requested = true;
                return 0;
            }
            if (wide_param == VK_SPACE)
            {
                if (application != NULL)
                {
                    application->simulation_is_paused = !application->simulation_is_paused;
                    application->simulation_single_step_requested = false;
                    Base_LogInfo("Simulation %s.", application->simulation_is_paused ? "paused" : "resumed");
                }
                return 0;
            }
            if (wide_param == 'N')
            {
                if (application != NULL && application->simulation_is_paused)
                {
                    application->simulation_single_step_requested = true;
                    Base_LogInfo("Simulation single-step requested.");
                }
                return 0;
            }
            if (wide_param == 'D')
            {
                if (application != NULL)
                {
                    application->particle_visualization_mode = SIMULATION_PARTICLE_VISUALIZATION_DENSITY;
                    Base_LogInfo("Visualization mode: %s", Application_GetVisualizationModeName(application->particle_visualization_mode));
                }
                return 0;
            }
            if (wide_param == 'V')
            {
                if (application != NULL)
                {
                    application->particle_visualization_mode = SIMULATION_PARTICLE_VISUALIZATION_VELOCITY;
                    Base_LogInfo("Visualization mode: %s", Application_GetVisualizationModeName(application->particle_visualization_mode));
                }
                return 0;
            }
            if (wide_param == 'H')
            {
                if (application != NULL)
                {
                    application->particle_visualization_mode = SIMULATION_PARTICLE_VISUALIZATION_SPATIAL_KEY;
                    Base_LogInfo("Visualization mode: %s", Application_GetVisualizationModeName(application->particle_visualization_mode));
                }
                return 0;
            }
            if (wide_param == 'B')
            {
                if (application != NULL)
                {
                    application->particle_visualization_mode = SIMULATION_PARTICLE_VISUALIZATION_BASIC;
                    Base_LogInfo("Visualization mode: %s", Application_GetVisualizationModeName(application->particle_visualization_mode));
                }
                return 0;
            }
            if (wide_param == 'M')
            {
                if (application != NULL)
                {
                    if (application->render_mode == SIMULATION_RENDER_MODE_PARTICLES)
                    {
                        application->render_mode = SIMULATION_RENDER_MODE_SCREEN_FLUID;
                    }
                    else
                    {
                        application->render_mode = SIMULATION_RENDER_MODE_PARTICLES;
                    }

                    Base_LogInfo("Render mode: %s", Application_GetRenderModeName(application->render_mode));
                }
                return 0;
            }
            if (wide_param == '7')
            {
                if (application != NULL)
                {
                    application->screen_fluid_visualization_mode = SIMULATION_SCREEN_FLUID_VISUALIZATION_COMPOSITE;
                    Base_LogInfo(
                        "Screen fluid view: %s",
                        Application_GetScreenFluidVisualizationModeName(application->screen_fluid_visualization_mode));
                }
                return 0;
            }
            if (wide_param == '8')
            {
                if (application != NULL)
                {
                    application->screen_fluid_visualization_mode = SIMULATION_SCREEN_FLUID_VISUALIZATION_PACKED;
                    Base_LogInfo(
                        "Screen fluid view: %s",
                        Application_GetScreenFluidVisualizationModeName(application->screen_fluid_visualization_mode));
                }
                return 0;
            }
            if (wide_param == '9')
            {
                if (application != NULL)
                {
                    application->screen_fluid_visualization_mode = SIMULATION_SCREEN_FLUID_VISUALIZATION_NORMAL;
                    Base_LogInfo(
                        "Screen fluid view: %s",
                        Application_GetScreenFluidVisualizationModeName(application->screen_fluid_visualization_mode));
                }
                return 0;
            }
            if (wide_param == 'I')
            {
                if (application != NULL)
                {
                    Application_LogSpatialHashInspection(application);
                }
                return 0;
            }
            if (wide_param == 'J')
            {
                if (application != NULL)
                {
                    Application_LogVolumeDensitySummary(application);
                }
                return 0;
            }
            if (wide_param == 'K')
            {
                if (application != NULL)
                {
                    SimulationRenderer_LogScreenFluidReadback(&application->renderer);
                }
                return 0;
            }
            if (wide_param == 'G')
            {
                if (application != NULL)
                {
                    if (application->screen_fluid_smoothing_mode == SIMULATION_SCREEN_FLUID_SMOOTHING_BILATERAL)
                    {
                        application->screen_fluid_smoothing_mode = SIMULATION_SCREEN_FLUID_SMOOTHING_GAUSSIAN;
                    }
                    else if (application->screen_fluid_smoothing_mode == SIMULATION_SCREEN_FLUID_SMOOTHING_GAUSSIAN)
                    {
                        application->screen_fluid_smoothing_mode = SIMULATION_SCREEN_FLUID_SMOOTHING_BILATERAL_2D;
                    }
                    else
                    {
                        application->screen_fluid_smoothing_mode = SIMULATION_SCREEN_FLUID_SMOOTHING_BILATERAL;
                    }

                    Base_LogInfo(
                        "Screen fluid smoothing: %s",
                        Application_GetScreenFluidSmoothingModeName(application->screen_fluid_smoothing_mode));
                }
                return 0;
            }
            if (wide_param == '1')
            {
                if (application != NULL)
                {
                    application->pipeline_settings.time_scale =
                        Base_ClampF32(application->pipeline_settings.time_scale - 0.1f, 0.1f, 4.0f);
                    Base_LogInfo("Simulation time scale: %.2f", application->pipeline_settings.time_scale);
                }
                return 0;
            }
            if (wide_param == '2')
            {
                if (application != NULL)
                {
                    application->pipeline_settings.time_scale =
                        Base_ClampF32(application->pipeline_settings.time_scale + 0.1f, 0.1f, 4.0f);
                    Base_LogInfo("Simulation time scale: %.2f", application->pipeline_settings.time_scale);
                }
                return 0;
            }
            if (wide_param == '3')
            {
                if (application != NULL)
                {
                    application->pressure_settings.pressure_multiplier =
                        Base_ClampF32(application->pressure_settings.pressure_multiplier - 1.0f, 0.0f, 60.0f);
                    application->pipeline_settings.pressure_settings = application->pressure_settings;
                    Base_LogInfo("Pressure multiplier: %.2f", application->pressure_settings.pressure_multiplier);
                }
                return 0;
            }
            if (wide_param == '4')
            {
                if (application != NULL)
                {
                    application->pressure_settings.pressure_multiplier =
                        Base_ClampF32(application->pressure_settings.pressure_multiplier + 1.0f, 0.0f, 60.0f);
                    application->pipeline_settings.pressure_settings = application->pressure_settings;
                    Base_LogInfo("Pressure multiplier: %.2f", application->pressure_settings.pressure_multiplier);
                }
                return 0;
            }
            if (wide_param == '5')
            {
                if (application != NULL)
                {
                    application->viscosity_settings.viscosity_strength =
                        Base_ClampF32(application->viscosity_settings.viscosity_strength - 0.02f, 0.0f, 1.0f);
                    application->pipeline_settings.viscosity_settings = application->viscosity_settings;
                    Base_LogInfo("Viscosity strength: %.2f", application->viscosity_settings.viscosity_strength);
                }
                return 0;
            }
            if (wide_param == '6')
            {
                if (application != NULL)
                {
                    application->viscosity_settings.viscosity_strength =
                        Base_ClampF32(application->viscosity_settings.viscosity_strength + 0.02f, 0.0f, 1.0f);
                    application->pipeline_settings.viscosity_settings = application->viscosity_settings;
                    Base_LogInfo("Viscosity strength: %.2f", application->viscosity_settings.viscosity_strength);
                }
                return 0;
            }
            return 0;
        }
    }

    return DefWindowProcA(window_handle, message, wide_param, long_param);
}

static bool Win32_RegisterWindowClass(HINSTANCE instance_handle)
{
    WNDCLASSEXA window_class = {0};
    window_class.cbSize = sizeof(window_class);
    window_class.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    window_class.lpfnWndProc = MainWindowProc;
    window_class.hInstance = instance_handle;
    window_class.hCursor = LoadCursorA(NULL, IDC_ARROW);
    window_class.lpszClassName = WINDOW_CLASS_NAME;

    return RegisterClassExA(&window_class) != 0;
}

static bool Win32_CreateWindowAndContext(Application *application, i32 client_width, i32 client_height)
{
    RECT window_rect = {0, 0, client_width, client_height};
    DWORD window_style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
    AdjustWindowRect(&window_rect, window_style, FALSE);

    application->window_handle = CreateWindowExA(
        0,
        WINDOW_CLASS_NAME,
        WINDOW_TITLE,
        window_style,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        window_rect.right - window_rect.left,
        window_rect.bottom - window_rect.top,
        NULL,
        NULL,
        application->instance_handle,
        application
    );

    if (application->window_handle == NULL) return false;

    application->device_context_handle = GetDC(application->window_handle);
    if (application->device_context_handle == NULL) return false;

    PIXELFORMATDESCRIPTOR pixel_format_descriptor = {0};
    pixel_format_descriptor.nSize = sizeof(pixel_format_descriptor);
    pixel_format_descriptor.nVersion = 1;
    pixel_format_descriptor.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pixel_format_descriptor.iPixelType = PFD_TYPE_RGBA;
    pixel_format_descriptor.cColorBits = 32;
    pixel_format_descriptor.cDepthBits = 24;
    pixel_format_descriptor.cStencilBits = 8;
    pixel_format_descriptor.iLayerType = PFD_MAIN_PLANE;

    int pixel_format = ChoosePixelFormat(application->device_context_handle, &pixel_format_descriptor);
    if (pixel_format == 0) return false;

    if (SetPixelFormat(application->device_context_handle, pixel_format, &pixel_format_descriptor) != TRUE)
    {
        return false;
    }

    application->rendering_context_handle = wglCreateContext(application->device_context_handle);
    if (application->rendering_context_handle == NULL) return false;

    if (wglMakeCurrent(application->device_context_handle, application->rendering_context_handle) != TRUE)
    {
        return false;
    }

    if (!gladLoadGL())
    {
        Base_LogError("gladLoadGL failed.");
        return false;
    }

    application->is_running = true;
    application->was_resized = true;

    ShowWindow(application->window_handle, SW_SHOW);
    UpdateWindow(application->window_handle);
    OpenGL_UpdateViewport(application);

    return true;
}

static void Win32_DestroyWindowAndContext(Application *application)
{
    Application_ShutdownSimulationResources(application);
    Simulation_ReleaseSpawnData(&application->initial_spawn_data);

    if (application->rendering_context_handle != NULL)
    {
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(application->rendering_context_handle);
        application->rendering_context_handle = NULL;
    }

    if (application->device_context_handle != NULL && application->window_handle != NULL)
    {
        ReleaseDC(application->window_handle, application->device_context_handle);
        application->device_context_handle = NULL;
    }

    if (application->window_handle != NULL)
    {
        DestroyWindow(application->window_handle);
        application->window_handle = NULL;
    }
}

static bool Application_InitializeSimulationView(Application *application)
{
    SimulationSpawnBox spawn_box = {0};
    spawn_box.center = Vec3_Create(0.0f, 0.0f, 0.0f);
    spawn_box.size = Vec3_Create(4.0f, 4.0f, 2.0f);
    spawn_box.particle_spacing = 0.16f;
    spawn_box.position_jitter_scale = 0.08f;
    spawn_box.initial_velocity = Vec3_Create(0.0f, 0.0f, 0.0f);

    if (!Simulation_GenerateSpawnDataBox(&application->initial_spawn_data, spawn_box))
    {
        return false;
    }

    application->simulation_bounds_size = Vec3_Create(8.0f, 8.0f, 8.0f);
    application->camera.target = Vec3_Create(0.0f, 0.0f, 0.0f);
    application->camera.distance = 11.0f;
    application->camera.yaw_radians = 0.6f;
    application->camera.pitch_radians = 0.35f;
    application->particle_visualization_mode = SIMULATION_PARTICLE_VISUALIZATION_BASIC;
    application->density_visualization_minimum = 0.0f;
    application->density_visualization_maximum = 1.0f;
    application->velocity_visualization_minimum = 0.0f;
    application->velocity_visualization_maximum = 1.0f;
    application->window_title_update_accumulator_seconds = 0.0f;
    application->most_recent_frame_delta_time_seconds = 0.0f;
    application->simulation_time_seconds = 0.0f;
    application->most_recent_whitewater_particle_count = 0u;
    application->render_mode = SIMULATION_RENDER_MODE_PARTICLES;
    application->screen_fluid_visualization_mode = SIMULATION_SCREEN_FLUID_VISUALIZATION_COMPOSITE;
    application->screen_fluid_smoothing_mode = SIMULATION_SCREEN_FLUID_SMOOTHING_GAUSSIAN;
    application->simulation_is_paused = false;
    application->simulation_single_step_requested = false;
    application->simulation_accumulator_seconds = 0.0f;
    application->fixed_simulation_delta_time_seconds = 1.0f / 60.0f;
    application->maximum_frame_delta_time_seconds = 1.0f / 15.0f;
    application->step_settings.gravity = -9.8f;
    application->step_settings.prediction_factor = 1.0f / 120.0f;
    application->step_settings.time_scale = 1.0f;
    application->step_settings.maximum_delta_time = 1.0f / 60.0f;
    application->step_settings.iterations_per_frame = 3;
    application->density_settings.smoothing_radius = 0.5f;
    application->collision_settings.bounds_size = application->simulation_bounds_size;
    application->collision_settings.collision_damping = 0.85f;
    application->pressure_settings.smoothing_radius = 0.5f;
    application->pressure_settings.target_density = 6.0f;
    application->pressure_settings.pressure_multiplier = 18.0f;
    application->pressure_settings.near_pressure_multiplier = 8.0f;
    application->viscosity_settings.smoothing_radius = 0.5f;
    application->viscosity_settings.viscosity_strength = 0.12f;
    application->spatial_hash_settings.cell_size = 0.5f;
    application->volume_density_settings.bounds_size = application->simulation_bounds_size;
    application->volume_density_settings.smoothing_radius = 0.5f;
    application->volume_density_settings.resolution_x = 24;
    application->volume_density_settings.resolution_y = 24;
    application->volume_density_settings.resolution_z = 24;
    application->volume_density_settings.density_scale = 1.0f;
    application->whitewater_settings.maximum_particle_count = 1000u;
    application->whitewater_settings.spawn_rate = 120.0f;
    application->whitewater_settings.spawn_rate_fade_in_time = 0.35f;
    application->whitewater_settings.spawn_rate_fade_start_time = 0.20f;
    application->whitewater_settings.trapped_air_velocity_minimum = 15.0f;
    application->whitewater_settings.trapped_air_velocity_maximum = 25.0f;
    application->whitewater_settings.kinetic_energy_minimum = 15.0f;
    application->whitewater_settings.kinetic_energy_maximum = 30.0f;
    application->whitewater_settings.target_density = application->pressure_settings.target_density;
    application->whitewater_settings.smoothing_radius = application->density_settings.smoothing_radius;
    application->whitewater_settings.gravity = application->step_settings.gravity;
    application->whitewater_settings.delta_time_seconds = application->fixed_simulation_delta_time_seconds;
    application->whitewater_settings.bubble_buoyancy = 1.4f;
    application->whitewater_settings.spray_classify_maximum_neighbors = 5;
    application->whitewater_settings.bubble_classify_minimum_neighbors = 15;
    application->whitewater_settings.bubble_scale = 0.3f;
    application->whitewater_settings.bubble_scale_change_speed = 7.0f;
    application->whitewater_settings.collision_damping = 0.10f;
    application->whitewater_settings.bounds_size = application->simulation_bounds_size;
    application->whitewater_settings.simulation_time_seconds = 0.0f;
    application->pipeline_settings.substeps_per_simulation_step = 3;
    application->pipeline_settings.time_scale = 1.0f;
    application->pipeline_settings.step_settings = application->step_settings;
    application->pipeline_settings.spatial_hash_settings = application->spatial_hash_settings;
    application->pipeline_settings.density_settings = application->density_settings;
    application->pipeline_settings.pressure_settings = application->pressure_settings;
    application->pipeline_settings.viscosity_settings = application->viscosity_settings;
    application->pipeline_settings.collision_settings = application->collision_settings;

    if (!Application_InitializeSimulationResources(application))
    {
        return false;
    }

    if (!Application_RunInitializationWarmup(application))
    {
        Application_ShutdownSimulationResources(application);
        return false;
    }

    Base_LogInfo("Particle renderer initialized with %u particles.", application->particle_buffers.particle_count);
    Base_LogInfo("Camera controls: arrow keys rotate, W/S zoom.");
    Base_LogInfo("Debug views: B basic, D density, V velocity, H spatial hash.");
    Base_LogInfo("Render controls: M toggles particles/screen-fluid, 7 composite, 8 packed, 9 normals, G cycles bilateral/gaussian/bilateral2d smoothing, K logs screen-fluid targets.");
    Base_LogInfo("Simulation controls: R resets, Space pauses, N single-steps, I logs hash inspection, J logs volume density.");
    Base_LogInfo("Whitewater: enabled with spawn/update modeled after Example Code.");
    Base_LogInfo("Runtime parameters: 1/2 time scale, 3/4 pressure, 5/6 viscosity.");
    return true;
}

static void Application_ShutdownSimulationResources(Application *application)
{
    Base_Assert(application != NULL);

    SimulationRenderer_Shutdown(&application->renderer);
    SimulationPipeline_Shutdown(&application->pipeline);
    SimulationStepper_Shutdown(&application->stepper);
    SimulationCollision_Shutdown(&application->collision);
    SimulationDensity_Shutdown(&application->density);
    SimulationPressure_Shutdown(&application->pressure);
    SimulationViscosity_Shutdown(&application->viscosity);
    SimulationSpatialHash_Shutdown(&application->spatial_hash);
    SimulationVolumeDensity_Shutdown(&application->volume_density);
    SimulationWhitewater_Shutdown(&application->whitewater);
    Simulation_DestroyParticleBuffers(&application->particle_buffers);
}

static bool Application_InitializeSimulationResources(Application *application)
{
    Base_Assert(application != NULL);

    if (!Simulation_CreateParticleBuffersFromSpawnData(&application->particle_buffers, &application->initial_spawn_data))
    {
        return false;
    }

    if (!SimulationWhitewater_Initialize(&application->whitewater, application->whitewater_settings.maximum_particle_count))
    {
        Application_ShutdownSimulationResources(application);
        return false;
    }

    if (!SimulationRenderer_Initialize(&application->renderer, &application->particle_buffers, &application->whitewater))
    {
        Application_ShutdownSimulationResources(application);
        return false;
    }

    if (!SimulationStepper_Initialize(&application->stepper))
    {
        Application_ShutdownSimulationResources(application);
        return false;
    }

    if (!SimulationSpatialHash_Initialize(&application->spatial_hash))
    {
        Application_ShutdownSimulationResources(application);
        return false;
    }

    if (!SimulationCollision_Initialize(&application->collision))
    {
        Application_ShutdownSimulationResources(application);
        return false;
    }

    if (!SimulationDensity_Initialize(&application->density))
    {
        Application_ShutdownSimulationResources(application);
        return false;
    }

    if (!SimulationPressure_Initialize(&application->pressure))
    {
        Application_ShutdownSimulationResources(application);
        return false;
    }

    if (!SimulationViscosity_Initialize(&application->viscosity))
    {
        Application_ShutdownSimulationResources(application);
        return false;
    }

    if (!SimulationPipeline_Initialize(&application->pipeline))
    {
        Application_ShutdownSimulationResources(application);
        return false;
    }

    if (!SimulationVolumeDensity_Initialize(&application->volume_density, application->volume_density_settings))
    {
        Application_ShutdownSimulationResources(application);
        return false;
    }

    if (!Application_RebuildSimulationDerivedState(application))
    {
        Application_ShutdownSimulationResources(application);
        return false;
    }

    return true;
}

static bool Application_RunInitializationWarmup(Application *application)
{
    Base_Assert(application != NULL);

    const i32 warmup_step_count = 36;
    const f32 warmup_delta_time_seconds = 1.0f / 120.0f;

    for (i32 warmup_step_index = 0; warmup_step_index < warmup_step_count; warmup_step_index++)
    {
        if (!Application_RunSimulationFixedStep(application, warmup_delta_time_seconds))
        {
            Base_LogError("Failed during simulation warmup.");
            return false;
        }
    }

    application->simulation_accumulator_seconds = 0.0f;
    application->simulation_single_step_requested = false;

    return true;
}

static bool Application_RebuildSimulationDerivedState(Application *application)
{
    bool rebuild_success = SimulationPipeline_RebuildDerivedState(
        &application->particle_buffers,
        &application->spatial_hash,
        &application->density,
        application->pipeline_settings);

    if (!rebuild_success)
    {
        Base_LogError("Failed to rebuild simulation derived state.");
        return false;
    }

    if (!SimulationVolumeDensity_Run(&application->volume_density, &application->particle_buffers, application->volume_density_settings))
    {
        Base_LogError("Failed to rebuild the volume density texture.");
        return false;
    }

    if (application->particle_visualization_mode == SIMULATION_PARTICLE_VISUALIZATION_DENSITY)
    {
        Application_UpdateDensityVisualizationRange(application);
    }
    else if (application->particle_visualization_mode == SIMULATION_PARTICLE_VISUALIZATION_VELOCITY)
    {
        Application_UpdateVelocityVisualizationRange(application);
    }

    return true;
}

static bool Application_RunSimulationFixedStep(Application *application, f32 simulation_delta_time_seconds)
{
    bool run_success = SimulationPipeline_RunSimulationStep(
        &application->pipeline,
        &application->stepper,
        &application->spatial_hash,
        &application->density,
        &application->pressure,
        &application->viscosity,
        &application->collision,
        &application->whitewater,
        &application->particle_buffers,
        application->pipeline_settings,
        &application->whitewater_settings,
        simulation_delta_time_seconds);

    if (!run_success)
    {
        Base_LogError("Stable simulation step failed.");
        return false;
    }

    if (!SimulationVolumeDensity_Run(&application->volume_density, &application->particle_buffers, application->volume_density_settings))
    {
        Base_LogError("Volume density update failed.");
        return false;
    }

    application->simulation_time_seconds += simulation_delta_time_seconds * application->pipeline_settings.time_scale;
    application->whitewater_settings.delta_time_seconds = simulation_delta_time_seconds * application->pipeline_settings.time_scale;
    application->whitewater_settings.simulation_time_seconds = application->simulation_time_seconds;
    application->whitewater_settings.smoothing_radius = application->pipeline_settings.density_settings.smoothing_radius;
    application->whitewater_settings.gravity = application->pipeline_settings.step_settings.gravity;
    application->whitewater_settings.target_density = application->pipeline_settings.pressure_settings.target_density;
    application->whitewater_settings.bounds_size = application->pipeline_settings.collision_settings.bounds_size;

    return true;
}

static void Application_ResetSimulation(Application *application)
{
    SimulationRenderMode previous_render_mode = application->render_mode;
    SimulationParticleVisualizationMode previous_visualization_mode = application->particle_visualization_mode;
    SimulationScreenFluidSmoothingMode previous_smoothing_mode = application->screen_fluid_smoothing_mode;

    Application_ShutdownSimulationResources(application);
    application->simulation_accumulator_seconds = 0.0f;
    application->simulation_single_step_requested = false;
    application->simulation_time_seconds = 0.0f;

    if (!Application_InitializeSimulationResources(application))
    {
        Base_LogError("Failed to reset simulation resources.");
        application->is_running = false;
        return;
    }

    if (!Application_RunInitializationWarmup(application))
    {
        application->is_running = false;
        return;
    }

    application->render_mode = previous_render_mode;
    application->particle_visualization_mode = previous_visualization_mode;
    application->screen_fluid_smoothing_mode = previous_smoothing_mode;

    Base_LogInfo("Simulation reset.");
}

static void Application_UpdateSimulation(Application *application, f32 frame_delta_time_seconds)
{
    bool ran_simulation_step = false;
    if (application->reset_requested)
    {
        application->reset_requested = false;
        Application_ResetSimulation(application);
        if (!application->is_running) return;
    }

    f32 clamped_frame_delta_time = Base_ClampF32(
        frame_delta_time_seconds,
        0.0f,
        application->maximum_frame_delta_time_seconds);

    if (application->simulation_is_paused)
    {
        if (application->simulation_single_step_requested)
        {
            application->simulation_single_step_requested = false;
            if (!Application_RunSimulationFixedStep(application, application->fixed_simulation_delta_time_seconds))
            {
                application->is_running = false;
                return;
            }
            ran_simulation_step = true;
        }
    }
    else
    {
        application->simulation_accumulator_seconds += clamped_frame_delta_time;

        while (application->simulation_accumulator_seconds >= application->fixed_simulation_delta_time_seconds)
        {
            if (!Application_RunSimulationFixedStep(application, application->fixed_simulation_delta_time_seconds))
            {
                application->is_running = false;
                return;
            }

            ran_simulation_step = true;
            application->simulation_accumulator_seconds -= application->fixed_simulation_delta_time_seconds;
        }
    }

    if (ran_simulation_step)
    {
        application->whitewater_settings.delta_time_seconds = clamped_frame_delta_time * application->pipeline_settings.time_scale;
        if (!SimulationWhitewater_UpdateOnly(&application->whitewater, &application->particle_buffers, application->whitewater_settings))
        {
            Base_LogError("Whitewater update failed.");
            application->is_running = false;
            return;
        }
    }
}

static void Application_UpdateDensityVisualizationRange(Application *application)
{
    u32 particle_count = application->particle_buffers.particle_count;
    if (particle_count == 0)
    {
        application->density_visualization_minimum = 0.0f;
        application->density_visualization_maximum = 1.0f;
        return;
    }

    Vec4 *density_values = (Vec4 *) malloc((size_t) particle_count * sizeof(Vec4));
    if (density_values == NULL)
    {
        return;
    }

    bool read_success = OpenGL_ReadBuffer(
        &application->particle_buffers.density_buffer,
        density_values,
        (i32) (particle_count * sizeof(Vec4)));

    if (!read_success)
    {
        free(density_values);
        return;
    }

    f32 density_minimum = density_values[0].x;
    f32 density_maximum = density_values[0].x;

    for (u32 particle_index = 1; particle_index < particle_count; particle_index++)
    {
        f32 density_value = density_values[particle_index].x;
        if (density_value < density_minimum) density_minimum = density_value;
        if (density_value > density_maximum) density_maximum = density_value;
    }

    free(density_values);

    application->density_visualization_minimum = density_minimum;
    application->density_visualization_maximum = density_maximum;
}

static void Application_UpdateVelocityVisualizationRange(Application *application)
{
    u32 particle_count = application->particle_buffers.particle_count;
    if (particle_count == 0)
    {
        application->velocity_visualization_minimum = 0.0f;
        application->velocity_visualization_maximum = 1.0f;
        return;
    }

    Vec4 *velocity_values = (Vec4 *) malloc((size_t) particle_count * sizeof(Vec4));
    if (velocity_values == NULL)
    {
        return;
    }

    bool read_success = OpenGL_ReadBuffer(
        &application->particle_buffers.velocity_buffer,
        velocity_values,
        (i32) (particle_count * sizeof(Vec4)));

    if (!read_success)
    {
        free(velocity_values);
        return;
    }

    f32 minimum_speed = Vec3_Length(Vec3_Create(velocity_values[0].x, velocity_values[0].y, velocity_values[0].z));
    f32 maximum_speed = minimum_speed;

    for (u32 particle_index = 1; particle_index < particle_count; particle_index++)
    {
        f32 speed = Vec3_Length(Vec3_Create(
            velocity_values[particle_index].x,
            velocity_values[particle_index].y,
            velocity_values[particle_index].z));

        if (speed < minimum_speed) minimum_speed = speed;
        if (speed > maximum_speed) maximum_speed = speed;
    }

    free(velocity_values);
    application->velocity_visualization_minimum = minimum_speed;
    application->velocity_visualization_maximum = maximum_speed;
}

static void Application_LogSpatialHashInspection(Application *application)
{
    u32 particle_count = application->particle_buffers.particle_count;
    if (particle_count == 0)
    {
        Base_LogInfo("Spatial hash inspection skipped: no particles.");
        return;
    }

    u32 sample_index = particle_count / 2u;
    Vec4 *position_values = (Vec4 *) malloc((size_t) particle_count * sizeof(Vec4));
    Vec4 *density_values = (Vec4 *) malloc((size_t) particle_count * sizeof(Vec4));
    Vec4 *velocity_values = (Vec4 *) malloc((size_t) particle_count * sizeof(Vec4));
    u32 *spatial_key_values = (u32 *) malloc((size_t) particle_count * sizeof(u32));
    u32 *spatial_hash_values = (u32 *) malloc((size_t) particle_count * sizeof(u32));
    u32 *spatial_offset_values = (u32 *) malloc((size_t) particle_count * sizeof(u32));

    bool allocation_success =
        position_values != NULL &&
        density_values != NULL &&
        velocity_values != NULL &&
        spatial_key_values != NULL &&
        spatial_hash_values != NULL &&
        spatial_offset_values != NULL;

    bool read_success = allocation_success &&
        OpenGL_ReadBuffer(
            &application->particle_buffers.predicted_position_buffer,
            position_values,
            (i32) (particle_count * sizeof(Vec4))) &&
        OpenGL_ReadBuffer(
            &application->particle_buffers.density_buffer,
            density_values,
            (i32) (particle_count * sizeof(Vec4))) &&
        OpenGL_ReadBuffer(
            &application->particle_buffers.velocity_buffer,
            velocity_values,
            (i32) (particle_count * sizeof(Vec4))) &&
        OpenGL_ReadBuffer(
            &application->particle_buffers.spatial_key_buffer,
            spatial_key_values,
            (i32) (particle_count * sizeof(u32))) &&
        OpenGL_ReadBuffer(
            &application->particle_buffers.spatial_hash_buffer,
            spatial_hash_values,
            (i32) (particle_count * sizeof(u32))) &&
        OpenGL_ReadBuffer(
            &application->particle_buffers.spatial_offset_buffer,
            spatial_offset_values,
            (i32) (particle_count * sizeof(u32)));

    if (!read_success)
    {
        Base_LogError("Spatial hash inspection failed.");
        free(position_values);
        free(density_values);
        free(velocity_values);
        free(spatial_key_values);
        free(spatial_hash_values);
        free(spatial_offset_values);
        return;
    }

    u32 sample_key = spatial_key_values[sample_index];
    u32 sample_hash = spatial_hash_values[sample_index];
    u32 bucket_start_index = (sample_key < particle_count) ? spatial_offset_values[sample_key] : particle_count;
    u32 bucket_particle_count = 0;

    if (bucket_start_index < particle_count)
    {
        for (u32 bucket_index = bucket_start_index; bucket_index < particle_count; bucket_index++)
        {
            if (spatial_key_values[bucket_index] != sample_key || spatial_hash_values[bucket_index] != sample_hash)
            {
                break;
            }

            bucket_particle_count += 1;
        }
    }

    Vec4 sample_position = position_values[sample_index];
    Vec4 sample_density = density_values[sample_index];
    Vec4 sample_velocity = velocity_values[sample_index];
    f32 sample_speed = Vec3_Length(Vec3_Create(sample_velocity.x, sample_velocity.y, sample_velocity.z));

    Base_LogInfo(
        "Spatial sample %u: position=(%.2f, %.2f, %.2f) speed=%.3f density=%.3f near_density=%.3f key=%u hash=%u bucket_start=%u bucket_count=%u",
        sample_index,
        sample_position.x,
        sample_position.y,
        sample_position.z,
        sample_speed,
        sample_density.x,
        sample_density.y,
        sample_key,
        sample_hash,
        bucket_start_index,
        bucket_particle_count);

    free(position_values);
    free(density_values);
    free(velocity_values);
    free(spatial_key_values);
    free(spatial_hash_values);
    free(spatial_offset_values);
}

static void Application_LogVolumeDensitySummary(Application *application)
{
    SimulationVolumeDensityReadbackSummary summary = {0};
    if (!SimulationVolumeDensity_ReadbackSummary(&application->volume_density, &summary))
    {
        Base_LogError("Volume density summary readback failed.");
        return;
    }

    Base_LogInfo(
        "Volume density: min=%.5f max=%.5f avg=%.5f center_slice_avg=%.5f center_voxel=%.5f resolution=%dx%dx%d",
        summary.minimum_density,
        summary.maximum_density,
        summary.average_density,
        summary.center_slice_average_density,
        summary.center_voxel_density,
        application->volume_density_settings.resolution_x,
        application->volume_density_settings.resolution_y,
        application->volume_density_settings.resolution_z);
}

static const char *Application_GetRenderModeName(SimulationRenderMode render_mode)
{
    switch (render_mode)
    {
        case SIMULATION_RENDER_MODE_PARTICLES: return "particles";
        case SIMULATION_RENDER_MODE_SCREEN_FLUID: return "screen fluid";
    }

    return "unknown";
}

static const char *Application_GetVisualizationModeName(SimulationParticleVisualizationMode particle_visualization_mode)
{
    switch (particle_visualization_mode)
    {
        case SIMULATION_PARTICLE_VISUALIZATION_BASIC: return "basic";
        case SIMULATION_PARTICLE_VISUALIZATION_DENSITY: return "density";
        case SIMULATION_PARTICLE_VISUALIZATION_VELOCITY: return "velocity";
        case SIMULATION_PARTICLE_VISUALIZATION_SPATIAL_KEY: return "spatial hash";
    }

    return "unknown";
}

static const char *Application_GetScreenFluidVisualizationModeName(SimulationScreenFluidVisualizationMode screen_fluid_visualization_mode)
{
    switch (screen_fluid_visualization_mode)
    {
        case SIMULATION_SCREEN_FLUID_VISUALIZATION_COMPOSITE: return "composite";
        case SIMULATION_SCREEN_FLUID_VISUALIZATION_PACKED: return "packed";
        case SIMULATION_SCREEN_FLUID_VISUALIZATION_NORMAL: return "normal";
    }

    return "unknown";
}

static const char *Application_GetScreenFluidSmoothingModeName(SimulationScreenFluidSmoothingMode screen_fluid_smoothing_mode)
{
    switch (screen_fluid_smoothing_mode)
    {
        case SIMULATION_SCREEN_FLUID_SMOOTHING_BILATERAL: return "bilateral";
        case SIMULATION_SCREEN_FLUID_SMOOTHING_GAUSSIAN: return "gaussian";
        case SIMULATION_SCREEN_FLUID_SMOOTHING_BILATERAL_2D: return "bilateral2d";
    }

    return "unknown";
}

// == OpenGL ==========================================================================================================

static void OpenGL_LogContextInfo(void)
{
    const char *vendor_name = (const char *) glGetString(GL_VENDOR);
    const char *renderer_name = (const char *) glGetString(GL_RENDERER);
    const char *version_name = (const char *) glGetString(GL_VERSION);
    GLint context_profile_mask = 0;
    const char *context_profile_name = "legacy/unknown";

    if (GLVersion.major > 3 || (GLVersion.major == 3 && GLVersion.minor >= 2))
    {
        glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &context_profile_mask);
        if ((context_profile_mask & GL_CONTEXT_CORE_PROFILE_BIT) != 0)
        {
            context_profile_name = "core";
        }
        else if ((context_profile_mask & GL_CONTEXT_COMPATIBILITY_PROFILE_BIT) != 0)
        {
            context_profile_name = "compatibility";
        }
    }

    Base_LogInfo("OpenGL vendor:   %s", vendor_name != NULL ? vendor_name : "Unavailable");
    Base_LogInfo("OpenGL renderer: %s", renderer_name != NULL ? renderer_name : "Unavailable");
    Base_LogInfo("OpenGL version:  %s", version_name != NULL ? version_name : "Unavailable");
    Base_LogInfo("GLAD parsed:     %d.%d", GLVersion.major, GLVersion.minor);
    Base_LogInfo("OpenGL profile:  %s (mask 0x%X)", context_profile_name, (u32) context_profile_mask);
    Base_LogInfo("Press Escape to close the window.");
}

static void OpenGL_UpdateViewport(Application *application)
{
    if (application->client_width <= 0 || application->client_height <= 0) return;

    glViewport(0, 0, application->client_width, application->client_height);
}

static void OpenGL_RenderFrame(Application *application, f32 delta_time_seconds)
{
    LARGE_INTEGER performance_frequency = {0};
    LARGE_INTEGER swap_start_counter = {0};
    LARGE_INTEGER swap_end_counter = {0};

    if (application->was_resized)
    {
        OpenGL_UpdateViewport(application);
        application->was_resized = false;
    }

    Application_UpdateSimulation(application, delta_time_seconds);
    if (!application->is_running) return;

    application->most_recent_frame_delta_time_seconds = delta_time_seconds;

    if (application->particle_visualization_mode == SIMULATION_PARTICLE_VISUALIZATION_DENSITY)
    {
        Application_UpdateDensityVisualizationRange(application);
    }
    else if (application->particle_visualization_mode == SIMULATION_PARTICLE_VISUALIZATION_VELOCITY)
    {
        Application_UpdateVelocityVisualizationRange(application);
    }
    SimulationRenderer_UpdateCamera(&application->camera, delta_time_seconds);
    SimulationRenderer_Render(
        &application->renderer,
        &application->particle_buffers,
        &application->whitewater,
        &application->volume_density,
        application->camera,
        application->simulation_bounds_size,
        application->render_mode,
        application->particle_visualization_mode,
        application->screen_fluid_visualization_mode,
        application->screen_fluid_smoothing_mode,
        application->density_visualization_minimum,
        application->density_visualization_maximum,
        application->velocity_visualization_minimum,
        application->velocity_visualization_maximum,
        application->client_width,
        application->client_height);
    QueryPerformanceFrequency(&performance_frequency);
    QueryPerformanceCounter(&swap_start_counter);
    SwapBuffers(application->device_context_handle);
    QueryPerformanceCounter(&swap_end_counter);
    application->most_recent_swap_buffers_milliseconds = (f32)
        (
            ((f64) (swap_end_counter.QuadPart - swap_start_counter.QuadPart) * 1000.0) /
            (f64) performance_frequency.QuadPart
        );

    application->window_title_update_accumulator_seconds += delta_time_seconds;
    if (application->window_title_update_accumulator_seconds >= 0.25f)
    {
        application->window_title_update_accumulator_seconds = 0.0f;
        Application_UpdateWindowTitle(application);
    }
}

// == Application Loop ================================================================================================

static void Application_ProcessMessages(Application *application)
{
    MSG message = {0};

    while (PeekMessageA(&message, NULL, 0, 0, PM_REMOVE))
    {
        if (message.message == WM_QUIT)
        {
            application->is_running = false;
        }

        TranslateMessage(&message);
        DispatchMessageA(&message);
    }
}

static void Application_Run(Application *application)
{
    LARGE_INTEGER performance_frequency = {0};
    LARGE_INTEGER previous_counter = {0};

    QueryPerformanceFrequency(&performance_frequency);
    QueryPerformanceCounter(&previous_counter);

    while (application->is_running)
    {
        LARGE_INTEGER current_counter = {0};
        QueryPerformanceCounter(&current_counter);

        f32 delta_time_seconds = (f32)
        (
            (double) (current_counter.QuadPart - previous_counter.QuadPart) /
            (double) performance_frequency.QuadPart
        );

        previous_counter = current_counter;
        Application_ProcessMessages(application);
        OpenGL_RenderFrame(application, delta_time_seconds);
    }
}

static void Application_UpdateWindowTitle(Application *application)
{
    const char *version_name = (const char *) glGetString(GL_VERSION);
    char title_buffer[256] = {0};
    f32 frame_rate = 0.0f;
    if (application->most_recent_frame_delta_time_seconds > 0.000001f)
    {
        frame_rate = 1.0f / application->most_recent_frame_delta_time_seconds;
    }

    if (!SimulationWhitewater_ReadActiveParticleCount(
            &application->whitewater,
            &application->most_recent_whitewater_particle_count))
    {
        application->most_recent_whitewater_particle_count = 0u;
    }

    if (version_name != NULL)
    {
        snprintf(
            title_buffer,
            sizeof(title_buffer),
            "FluidSim | %s | %s | %.0f FPS | sim %.2f ms | rnd %.2f ms | swap %.2f | grnd %.2f | gblur %.2f | gcomp %.2f | hash %.2f | dens %.2f | pres %.2f | ww %u | GL %s",
            application->simulation_is_paused ? "paused" : "running",
            Application_GetRenderModeName(application->render_mode),
            frame_rate,
            (f32) application->pipeline.last_debug_timings.total_milliseconds,
            (f32) application->renderer.last_debug_timings.total_milliseconds,
            application->most_recent_swap_buffers_milliseconds,
            (f32) application->renderer.last_debug_timings.gpu_total_milliseconds,
            (f32) application->renderer.last_debug_timings.gpu_blur_milliseconds,
            (f32) application->renderer.last_debug_timings.gpu_composite_milliseconds,
            (f32) application->pipeline.last_debug_timings.spatial_hash_milliseconds,
            (f32) application->pipeline.last_debug_timings.density_milliseconds,
            (f32) application->pipeline.last_debug_timings.pressure_milliseconds,
            application->most_recent_whitewater_particle_count,
            version_name);
    }
    else
    {
        snprintf(title_buffer, sizeof(title_buffer), "FluidSim - OpenGL Active");
    }

    SetWindowTextA(application->window_handle, title_buffer);
}
