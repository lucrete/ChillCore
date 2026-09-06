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

No consumer. Volumetric approaches and 3D noise would want it, and neither is planned.

Also missing, and mentioned here so it is not rediscovered separately: cubemap arrays.

### Depth is not available to post-processing

The scene target carries a depth attachment, and it is destroyed unused: only the colour texture is handed to the post-process stack.

Passing it is a small change, and it is what unblocks the effects usually asked for — depth of field, volumetric fog, and any depth-aware variant of an existing effect. Worth more than the chain above, and independent of it.

Needs a decision on whether depth is exposed as an ordinary sampled texture or as something the stack describes, since a depth format is not filterable the way a colour texture is.

### Occlusion culling

Rejecting draws hidden behind other geometry, as opposed to draws outside the view. Frustum culling and the depth pre-pass are in plan (`RenderingPlan.md`); neither covers this.

**Deferred deliberately, not blocked.** The two planned items take most of the value first. Frustum culling removes what is off screen, and the pre-pass removes the *shading* cost of what is occluded — the remainder here is the draw call and the vertex processing of hidden objects. That remainder is real at production scene complexity and small below it, and this is the most complex of the three by a distance.

**Three approaches, none obviously right.**

- **Hardware occlusion queries.** Draw an object's bounds, ask how many samples passed. Simple to express and needs no new engine concepts, but the answer arrives a frame or more later. Reading it back in the same frame stalls the pipeline, which costs more than the overdraw it saves; using the previous frame's answer means accepting objects that pop in for a frame. There is no query surface in `Gfx::RenderApi` today.
- **Hierarchical depth buffer.** Build a mip chain over the depth the pre-pass already produces, then test bounds against the coarsest level that covers them. No stall if the test and the draw both stay on the GPU, which needs compute to run the test and indirect draw to consume the result. `DrawIndirect` and `supportsIndirectDraw` exist in the abstraction; compute is reachable only once its planned work lands.
- **Software occlusion rasterisation.** Rasterise a small authored occluder set on the CPU and test bounds against it. No GPU dependency, no latency, no capability gate. Costs CPU time on a thread that is not currently doing anything else, and requires occluders to be authored per scene, which is a content pipeline obligation rather than a code one.

**What it needs first, regardless of approach.** Bounding volumes, which frustum culling introduces, and a populated depth buffer ahead of the draws being culled, which the pre-pass produces. Both are prerequisites that fall out of planned work rather than costs of this item.

**Worth stating plainly.** Occlusion culling is the one optimisation here that can lose outright: every rejected object saves a draw, every survivor pays the test, and a scene whose geometry is mostly visible pays all of the cost for none of the benefit. It earns a place when a scene has real occluders — interiors, terrain, dense urban geometry — and not before.

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

### Photometric light units

Light intensities are authored numbers, not physical units. `LightDirectional` holds a bare `intensity` float the PBR shader multiplies the BRDF by, so `2.0` means "twice 1.0" and nothing else. There is no answer to "how bright is the sun" beyond whatever looked right.

**Deferred deliberately, not blocked.** Linear rendering was the prerequisite and it has landed: units only compose if the maths between them is linear. Nothing requires physical units yet, and the payoff is realism and authoring discipline rather than a feature. It earns its place when lighting has to match a reference photograph, or when a day/night cycle needs sun, moon and interior lamps to coexist at credible relative brightness — eyeballed multipliers stop scaling there.

**Photometric, not radiometric.** Radiometric units measure raw energy in watts. Photometric units are the same quantities weighted by human eye sensitivity, so green counts for more than deep red at equal power. Realtime rendering wants the photometric set because the output is an image for a person.

**Each light is expressed in its own unit, never a shared intensity.** A number that does not name its unit is not transferable, which is the whole point of the change. Which unit applies depends on the light's shape.

| Light | Control | Range | Reference default |
| --- | --- | --- | --- |
| Directional | `Illuminance (lx)` | 0 – 130,000, logarithmic | 100,000, direct sun |
| Point | `Luminous flux (lm)` | 0 – 20,000, logarithmic | 1,600, a 100 W-equivalent bulb |
| Spot | `Luminous flux (lm)` | 0 – 20,000, logarithmic | 1,600 |
| Emissive material | `Luminance (nits)` | 0 – 10,000, logarithmic | 100, a lit screen |

**Logarithmic sliders are required, not a nicety.** A linear control over 0 – 130,000 spends nearly all its travel where everything is blown out, and leaves the range an interior lamp occupies too small to hit. Brightness is perceived roughly logarithmically, so the control should be too; a stop-based slider, each notch doubling, is the form artists already know.

**Lumens is the authoring unit, candela the internal one.** The number on the bulb box is lumens, and the engine converts — `candela = lumens / 4π` for a point light. Spot cones force a decision that will confuse whoever tunes the second spot if it is made silently: holding **lumens** constant means narrowing the cone concentrates the same light and brightens the pool, which is what a physical reflector does; holding **candela** constant means narrowing only shrinks the pool. Film-lighting tools offer both. Lumens, with cone angle visibly affecting brightness, is the physical behaviour and matches the box.

**Colour has to stop carrying brightness.** The shader currently multiplies `lightColor * lightIntensity`, so a colour of `(0.5, 0.5, 0.5)` halves the output and makes a stated lux value a lie. Colour becomes pure chromaticity, normalised so it cannot change the measured quantity, and the natural control is `Colour temperature (K)` over 1,500 – 15,000 with a swatch: 1,900 K candle, 2,700 K tungsten, 6,500 K daylight. Brightness then lives in exactly one field.

**Reference values have to be within reach.** Nobody remembers that overcast daylight is 10,000 lx. A preset list beside the field that fills in a named value — "Overcast, 10,000 lx", "Office, 500 lx" — while leaving the number editable is the difference between the unit helping and it being extra typing.

**Exposure becomes a camera property.** With the sun at 100,000, something must map the scene to the range the display accepts, and under physical units that something is a real camera: aperture, shutter speed and ISO combined into an EV100 value yielding the scale. It replaces the `exposure` parameter that currently sits on the tone-map effect in the post-process stack, so the work crosses two subsystems.

**The cost is re-authoring, not the shader change.** Every existing scene's light values become wrong the moment the unit changes, and no mechanical conversion exists because the present numbers encode nothing. Bloom thresholds, the emissive ladder in the example scene, and the ambient term are all restated with them. Values in the tens of thousands also put more weight on the tone curve and on float precision than the current small range does.

**Also note.** `LightDirectional::intensity` has a getter and no setter, and there is no light tuning panel at all, so the controls above are new surface either way. `PostProcessEffect` already solves the problem these panels pose — each effect declares its own parameter names, types, ranges and defaults, and the developer panel builds itself from that description — and lights would want the same treatment rather than a panel hardcoded per light type.

**Done when:** lights are specified in physical units, exposure is a camera property, and shipped scenes are re-authored against both.

### Camera registration by name

`CameraManager` keys its map by `std::string` and asserts rather than storing a null. `GetViewProjectionMatrix` and `GetCameraPosition` dereference `activeCamera` unguarded. No state registers a second camera beyond the manager's own default, so the F9 free-camera toggle is a no-op outside it. No longer crashes; not urgent.

---

## AudioTracker

### Editor first-cuts to replace

- Track source assignment is a "Cycle Source" button (`AudioTrackerController.cpp:583`). The intended dropdown is blocked on `UiDropdown` supporting dynamically populated options.
- Neither grid shows a playhead. `TrackerClock` publishes its position for exactly this purpose and nothing reads it for display.
- The non-Windows storage path is a placeholder (`TrackerPaths.h:14`), pending a second desktop target.

### Editor polish

Deferred as a group, none blocking anything. Each is a follow-up rather than part of the initial cut.

- Per-track gain sliders and solo. Muting works in both views; gain and solo do not.
- Variable step subdivision — eighths, sixteenths, thirty-seconds, triplets.
- Editing pattern length after creation.
- Waveform display for samples in the panel.
- Keyboard shortcuts, including play and stop, and direct track selection.
- Tempo and time-signature changes mid-song, with an automation lane.
- Offline render to an audio file through the existing engine.

---

## Platform and build

### Android platform gaps

Three file-system operations are unimplemented on Android (`PlatformFileSystemAndroid.cpp:158-172`). Deliberate — no Android consumer today.

- `CopyFile` and `ListDirectoryEntries` both return failure, which blocks any Android feature needing to enumerate files.
- `WriteFileTextAtomic` forwards to an ordinary write and is not atomic. Anything on Android that comes to depend on the atomicity guarantee needs this addressed first, which requires scoped-storage work.

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
