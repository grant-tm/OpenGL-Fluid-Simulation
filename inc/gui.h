#ifndef GUI_H
#define GUI_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "simulation_collision.h"
#include "simulation_pipeline.h"
#include "simulation_pressure.h"
#include "simulation_renderer.h"
#include "simulation_step.h"
#include "simulation_viscosity.h"
#include "simulation_whitewater.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GuiFrameData
{
    struct GuiSimulationTimings
    {
        f64 external_forces_milliseconds;
        f64 spatial_hash_milliseconds;
        f64 reorder_milliseconds;
        f64 density_milliseconds;
        f64 pressure_milliseconds;
        f64 viscosity_milliseconds;
        f64 collision_milliseconds;
        f64 integrate_positions_milliseconds;
        f64 total_milliseconds;
    } simulation_timings;
    struct GuiRendererTimings
    {
        f64 bounds_milliseconds;
        f64 particles_milliseconds;
        f64 scene_copy_milliseconds;
        f64 shadow_milliseconds;
        f64 surface_inputs_milliseconds;
        f64 prepare_milliseconds;
        f64 blur_milliseconds;
        f64 normal_milliseconds;
        f64 composite_milliseconds;
        f64 whitewater_milliseconds;
        f64 total_milliseconds;
        f64 gpu_scene_copy_milliseconds;
        f64 gpu_shadow_milliseconds;
        f64 gpu_surface_inputs_milliseconds;
        f64 gpu_blur_milliseconds;
        f64 gpu_normal_milliseconds;
        f64 gpu_composite_milliseconds;
        f64 gpu_total_milliseconds;
    } renderer_timings;
    bool *simulation_is_paused;
    bool *simulation_single_step_requested;
    bool *reset_requested;
    bool *frame_dump_is_active;
    bool *frame_dump_request_start;
    bool *frame_dump_request_stop;
    bool *frame_dump_capture_gui;
    i32 *frame_dump_total_frame_count;
    i32 *frame_dump_frames_per_second;
    char *frame_dump_output_directory;
    i32 frame_dump_output_directory_capacity;
    SimulationStepSettings *step_settings;
    SimulationPressureSettings *pressure_settings;
    SimulationViscositySettings *viscosity_settings;
    SimulationCollisionSettings *collision_settings;
    SimulationWhitewaterSettings *whitewater_settings;
    SimulationScaleModel *scale_model;
    SimulationRenderMode *render_mode;
    SimulationParticleVisualizationMode *particle_visualization_mode;
    SimulationScreenFluidVisualizationMode *screen_fluid_visualization_mode;
    SimulationScreenFluidSmoothingMode *screen_fluid_smoothing_mode;
    f32 current_frames_per_second;
    f32 current_frame_delta_time_milliseconds;
    f32 current_swap_buffers_milliseconds;
    u32 whitewater_active_count;
    u32 main_particle_count;
    u32 frame_dump_written_frame_count;
} GuiFrameData;

bool Gui_Initialize(HWND window_handle);
void Gui_Shutdown(void);
void Gui_BeginFrame(void);
void Gui_Draw(GuiFrameData *frame_data);
void Gui_Render(void);
bool Gui_HandleWindowMessage(HWND window_handle, UINT message, WPARAM wide_param, LPARAM long_param);
bool Gui_WantsCaptureKeyboard(void);
bool Gui_WantsCaptureMouse(void);

#ifdef __cplusplus
}
#endif

#endif // GUI_H
