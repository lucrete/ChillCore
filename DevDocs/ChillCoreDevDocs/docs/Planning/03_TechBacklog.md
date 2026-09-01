# 03 — Tech Backlog

Work identified but not yet in plan. Summaries only. When an item is picked up it moves to `02_Roadmap.md` and gets a section in its workstream planning doc — see `01_DevProcess.md`. Per `Guidelines/CodingGuidelines.md`, future work is captured here rather than in `// Todo` comments.

Last reviewed against the source: 2026-09-01.

---

## Rendering

### A caller-defined post-process effect chain

Effects fuse into one pass in an order fixed by the shader. A caller can neither reorder them nor insert one of its own.

**Deferred deliberately, not blocked.** There is no consumer: nothing in the engine or the application wants a custom post-process effect today. It moves back into plan when a feature needs it.

**Reordering the built-ins should not be granted.** The order encodes colour science — the grade runs before the tone curve in log space so a preset stays portable across exposures, and the vignette runs in linear because it is a lens effect on incoming light rather than a darkening of a finished image. Exposing reordering lets a caller make that wrong. Neither commercial engine offers it either; both offer injection points instead, naming where in the pipeline an effect runs rather than permuting the built-ins.

**Three ways to allow insertion.**

- **A pass per effect.** Fully general, and costs a full frame read and write each. Rejected for the reason the pass was fused in the first place: on a tiled GPU that bandwidth is the dominant cost.
- **Runtime shader permutation.** Assemble the fused shader from snippets in the requested order, cache a variant per unique chain. The only option giving fusion and free ordering together. Brings runtime shader compilation, a variant cache, a generated parameter-block layout, and forces every effect to be authored as a snippet rather than a shader.
- **A fixed fused core plus injection points.** Built-ins stay fused in their current order; a caller's effect runs as its own pass at a named point. The caller pays one pass for its own effect knowingly, the built-ins cost nothing extra, and nothing is compiled at runtime. The preferred shape.

**What a chain entry would be.** A shader plus parameters — concretely a name, a material, an injection point and an enable flag, with the material owned by the caller. The stack owns a pair of full-resolution ping-pong targets, allocated only when an entry exists; an entry reads one and writes the other.

Bloom is the case to design against, and the conclusion is that it does not fit: it needs half-resolution targets of its own and three passes feeding each other. An effect needing its own resolution or pass count is a built-in, not a chain entry. Depth of field and motion blur would land the same way.

**What it would actually enable.** Less than it appears, because the stack receives scene colour only:

- **After the post pass**, on display-referred colour: film grain, scanlines and CRT emulation, pixelation, posterise, dither, halftone, sharpen, screen wipes and transitions, damage flashes, letterboxing, colour-blindness simulation. All achievable, all cheap. This is the half that carries the value.
- **Before the post pass**, on linear colour: chromatic aberration, radial blur, lens distortion, glare streaks. The effects usually wanted here — depth of field, ambient occlusion, screen-space reflections, motion blur, volumetric fog — all need depth or velocity and are blocked regardless of the chain.

**Weigh against the cheap alternative.** Adding a built-in effect costs a gated block in the fused shader and a parameter list: no new architecture, no extra pass, no extra bandwidth. For an engine-owned effect that is already easy. The chain earns its keep only when an effect must come from outside the engine — an application-defined effect, or a scene declaring its own shader — which is a real architectural goal but not a current need.

**If picked up, consider descoping** to the after-the-pass injection point alone. It is roughly half the work and drops the point whose interesting uses are blocked anyway.

### 3D textures cannot be created

Texture creation covers 2D, array and cubemap shapes. `depth` is present on the description, asserted to 1, and otherwise ignored.

Small: the shape branch and the storage call already exist for array textures, and the layered attachment path already handles selecting a slice, so a 3D texture would attach with no further work. Both backends carry identical copies of this code, so it is written twice.

No consumer. The only named one is the 3D route to a baked grade table, which has a 2D strip alternative needing nothing new. Volumetric approaches and 3D noise would want it, and neither is planned.

Also missing, and mentioned here so it is not rediscovered separately: cubemap arrays.

### Depth is not available to post-processing

The scene target carries a depth attachment, and it is destroyed unused: only the colour texture is handed to the post-process stack.

Passing it is a small change, and it is what unblocks the effects usually asked for — depth of field, volumetric fog, and any depth-aware variant of an existing effect. Worth more than the chain above, and independent of it.

Needs a decision on whether depth is exposed as an ordinary sampled texture or as something the stack describes, since a depth format is not filterable the way a colour texture is.

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
