# NextGen
## Introduction
NextGen is a special-purpose 3D rendering engine written in modern C++. It uses DirectX 12 exclusively as rendering API.
## Requirements
### Build
* Visual Studio 2019 16.7.X (older versions are not supported) with C++ development workload and Windows 10 SDK version 10.0.19041.0.
### Run
* Windows 10 v1809
* DX12 GPU/driver with suport for the following features:
	* Resource binding tier 2 or higher (unlimited texture count accessible in shader)
	* Shader Model 6.0
	* HLSL SIMD ops
	* root signature 1.1
	* `WriteBufferImmediate`
* CPU instruction extensions:
	* SSE
	* AVX
	* popcnt (for debug builds)
## Current implementation status
* Vector math library with HLSL-like syntax including swizzles. Incorporates compile-time WAR hazard detector for copy elision optimization.
* BVH with configurable structure - quadtree/enneatree (2D), octree/icoseptree (3D), and 2 split techniques - regular/mean.
* Batched terrain vector data rendering subsystem. All objects in a BVH node rendered in 1 draw call.
* Novel temporal 2-phase occlusion culling technique. Screen areas without previous frame occlusion information estimated via AABB occlusion accumulation in stencil buffer.
* Render pipeline with multithreaded stages building.
* Multithreaded BVH traverse, including frustum culling and occlusion culling benefit estimation (which is used to reduce number of nodes for which occlusion culling will be performed, it rejects nodes that are probably visible or containing too small geometry compared to its size), it implemented with SSE (2D) and AVX (3D) intrinsics.
* Multithreaded command lists recording. Number of threads is limited in order to avoid CPU oversubscription.
* Rendering commands for objects consisting of several subobjects prerecorded into bundles. It results in 1 `ExecuteBundle` call per object during direct command list recording.
* Batched GPU work submission.
* Automated barrier batching system.
* Render passes support - brings GPU perf boost (especially for tiled architectures).
* Resource GPU lifetime tracking and frame versioning.
* Resource allocation optimizations (e.g. place vertex and index data into single buffer).
* Threadsafe VRAM allocators with hybrid sync approach: lock-free vast majority of time, blocking on new chunk allocation or ring buffer wrap.
* PBR lighting using GGX NDF with height-direction-correlated Smith G term for specular and GGX diffuse approximation. Bidirectional Fresnel specular<->diffuse interface.
* Adaptive specular SSAA.
* Direct sun lighting with atmospheric Rayleigh scattering.
* Adaptive autoexposure/tonemapping. Wave intrinsics from Shader Model 6.0 used to accelerate final reduction steps in log average and max luminance calculation process.
* Lens flare.
* Bokeh DOF with autofocus. 4-layer sprite based scattering with diaphragm edge antialiasing and background reconstruction.
* MSAA/CSAA/EQAA antialiasing with HDR-aware resolve.
