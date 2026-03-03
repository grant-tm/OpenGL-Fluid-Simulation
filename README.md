# FluidSim

`FluidSim` is a native Win32/OpenGL fluid simulation demo written in C, with Dear ImGui used for live controls and diagnostics.

<table>
  <tr>
    <td><img src="assets/images/demo.gif" width="100%"></td>
    <td><img src="assets/images/demo_alt.gif" width="100%"></td>
  </tr>
</table>

The project is an attempted direct translation of [Sebastian Lague's Unity fluid simulation](https://github.com/SebLague/Fluid-Sim) into C / OpenGL, with mixed success. I tried to reimplmement the techniques in the reference project as faithfully as possible, so any success of my simulation owes all credit to Sebastian Lague, and all shortcomings are my own.

## What It Does

- Simulates a 3D particle-based fluid
- Renders the fluid with a screen-space surface reconstruction pipeline
- Generates secondary whitewater particles for spray / foam / bubbles
- Provides live tuning and debug views through an ImGui control surface

## Core Simulation Technique

The fluid sim is a particle-based SPH-style solver.

Simulation pipeline (per-step):

1. Apply external forces and update predicted positions
2. Generate a spatial hash from predicted positions
3. Sort particles by spatial key on the GPU
4. Reorder particle attributes on the GPU to match the sorted key order
5. Compute density and near-density from neighboring particles
6. Apply pressure and near-pressure forces
7. Apply viscosity
8. Resolve collisions against the simulation bounds
9. Commit predicted positions back into the main particle state

## Rendering Technique

The active primary renderer is a screen-space fluid surface.

Render path:

1. Render particle thickness
2. Render particle depth
3. Prepare a packed screen-space representation
4. Smooth the packed data
5. Reconstruct normals from smoothed depth
6. Composite the final fluid surface
7. Integrate foam / whitewater into the screen-space composite

## Whitewater

The project includes a whitewater system over top of the regular fluid simulation.
This is the least successful aspect of the simulation; no combination of modifiable whitewater parameters I expose looks as realistic as the reference simulation.

Whitewater pipeline:

- spawn during the pressure pass
- classify/update as `spray`, `foam`, or `bubble`
- render into a dedicated foam texture
- composite foam into the screen-space fluid renderer

## GUI And Diagnostics

The application includes a live control surface with panels for:

- simulation parameters
- rendering parameters
- diagnostics and timings

The GUI supports live inspection of:

- fluid parameters
- whitewater parameters
- render modes
- smoothing modes
- counts
- CPU and GPU timings

There are also multiple debug visualizations for:

- density
- velocity
- spatial hashing
- packed screen-space data
- normals
- hard / smooth / delta depth
- foam
- whitewater class views
- whitewater spawn-term views

## Build

Requirements:

- Windows
- CMake 3.20+
- MSVC / Visual Studio with C and C++ toolchains
- OpenGL-capable GPU / driver

The project uses:

- Win32
- OpenGL
- GLAD
- Dear ImGui as a git submodule in [`external/imgui`](C:/Repos/FluidSim/external/imgui)

Configure and build:

```powershell
cmake -S . -B build
cmake --build build --config Debug
```

Run:

```powershell
build\bin\Debug\FluidSim.exe
```

Assets are copied to the output directory automatically by [`CMakeLists.txt`](C:/Repos/FluidSim/CMakeLists.txt).