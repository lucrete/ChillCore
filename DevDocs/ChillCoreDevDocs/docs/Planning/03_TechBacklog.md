# 03 — Tech Backlog

Work identified but not yet in plan. Summaries only. When an item is picked up it moves to `02_Roadmap.md` and gets a section in its workstream planning doc — see `01_DevProcess.md`. Per `Guidelines/CodingGuidelines.md`, future work is captured here rather than in `// Todo` comments.

Last reviewed against the source: 2026-09-01.

---

## Rendering

### Specular antialiasing

Specular highlights shimmer under motion. Measured on the helmet model over a rotation sweep of 0.35 degrees per step: peak per-pixel change of 219 of 255 between adjacent steps, with bloom disabled.

**Cause.** A pixel covers a footprint of the surface, not a point. The normal varies across that footprint — from curvature, and from normal-map detail finer than a pixel. A GGX lobe is narrow at low roughness, so specular is strongly non-linear in the normal, and shading once at the average normal is not the average of shading over the footprint. The result is a spike when the sampled normal aligns with the half-vector and nothing when it does not.

Multisampling does not address it. Multisampling stores several coverage and depth samples per pixel but runs the fragment shader once per pixel per triangle, so it antialiases where triangles end. This aliasing is entirely within a triangle. Supersampling would fix it by shading every pixel several times, which is why nobody ships it.

**Approach.** Widen the lobe to match the footprint rather than sampling it more. A narrow GGX lobe convolved with a spread of normals is approximately a wider GGX lobe, so the variance of normals within a pixel can be folded into roughness and the surface shaded once — roughly `alpha_filtered = alpha + 2 * variance`. Three sources of that variance, in increasing cost:

- **Geometric specular antialiasing** — screen-space derivatives of the shading normal. Estimate variance from `dFdx(N)` and `dFdy(N)`. No new resources, under a dozen lines in the fragment shader, captures curvature. The first move. Kaplanyan and others, *Filtering Distributions of Normals for Shading Antialiasing*; refined by Tokuyoshi and Kaplanyan, *Improved Geometric Specular Antialiasing*.
- **Toksvig** — normal-map mip variance. Mipmap normal maps without renormalising, then use the length of the averaged normal as the spread: a length below one means the normals disagreed. Catches sub-pixel normal-map detail that derivatives miss. Requires the texture pipeline to stop renormalising. Toksvig, *Mipmapping Normal Maps*.
- **LEAN mapping** — first and second moments of the normal distribution stored per texel, which handles anisotropic spread properly. More storage and more machinery; not worth it before the other two are exhausted. Olano and Baker, *LEAN Mapping*.

Implementation sits in `Code/App/Data/Shaders/Rendering/pbr.glsl`: the shading normal is already resolved there before the BRDF, so the first approach needs no new inputs. It composes with the minimum-roughness clamp already in that shader, because it only ever raises effective roughness.

**Cost.** Tight highlights read slightly duller where the surface curves fast or the normal map is busy. That is the trade: a physically correct highlight that cannot be sampled, exchanged for a slightly soft one that is stable.

**Alternative.** Temporal antialiasing covers all shading aliasing rather than specular alone, by jittering the sub-pixel sample position per frame and accumulating with reprojection. It needs motion vectors, a history buffer, and neighbourhood clamping against ghosting, none of which exist. Larger piece of work, and it would supersede this one.

**Done when:** the peak per-pixel change over the same rotation sweep drops substantially with bloom disabled, and tight highlights on low-roughness surfaces remain recognisably tight.

### Camera registration by name

`CameraManager` keys its map by `std::string` and asserts rather than storing a null. `GetViewProjectionMatrix` and `GetCameraPosition` dereference `activeCamera` unguarded. No state registers a second camera beyond the manager's own default, so the F9 free-camera toggle is a no-op outside it. No longer crashes; not urgent.

---

## AudioTracker

### Editor first-cuts to replace

- Track source assignment is a cycle button; the intended dropdown is blocked on `UiDropdown` supporting dynamically populated options.
- Neither grid shows a playhead, though the transport publishes its position for exactly this.
- The non-Windows storage path is a placeholder, pending a second desktop target.

### Editor polish

Follow-ups, none blocking anything: per-track gain and solo, variable step subdivision, pattern-length editing after creation, sample waveform display, keyboard shortcuts, mid-song tempo and time-signature automation, offline render to an audio file.

---

## Platform and build

### Android platform gaps

`CopyFile` / `ListDirectoryEntries` return failure and `WriteFileTextAtomic` is not atomic on Android. Deliberate — no Android consumer today — but blocks any Android feature needing directory enumeration or the atomicity guarantee. Detail in `PlatformAndBuildPlan.md`.

### Android through the pipeline

The pipeline builds the desktop target only; Android is still built by invoking Gradle directly. Adding it is a target-selection option and a Gradle invocation reporting into the same folder structure. Unblocked, low value until something other than a developer's machine builds the package.

### WebGL backend

A third graphics backend under emscripten, plus platform backends for window, file access, and input over the web runtime. Capability-gates compute, storage buffers, and indirect draw off — dependent states need a fallback or must report unsupported. Not started.

### Further graphics backends

Vulkan, then Direct3D, then Metal if Apple platforms become a real target. Largely mechanical — the abstraction was designed for them, and they inherit a shim-free `Gfx::RenderApi`. Order open; Vulkan first is likely, being cross-platform and applicable to Android.

### Shader cross-compilation

Author in one language, compile to an intermediate representation, translate to each target (glslang → SPIR-V → SPIRV-Cross). Deferred deliberately: AGD-0080 holds this until a second shader target ships. Android currently gets its dialect differences through `ShaderManager` preamble injection. Revisit when WebGL or Vulkan lands.

---

## Open design questions

Decisions nobody has needed to make yet, recorded so they are not rediscovered from scratch. Resolving one means writing the decision into the relevant AGD.

### Material versus material instance

Materials are shared objects with no per-renderable override. Setting a colour on one object sets it on every object using that material, and a per-object override would break material-based batching and sorting. The usual answer is a material instance holding per-object overrides over a shared base. Nothing has needed it yet. Adjacent to the batching and sort code — worth resolving whenever that is next open.

---

## Documentation debt

- AGD-0120 (UI Layout and Styling) inherited a provisional design; needs a pass to confirm or revise it.
- `mkdocs.yml` has no `nav`. Deliberate — navigation is derived from the folder structure. Recorded so it is not "fixed".
