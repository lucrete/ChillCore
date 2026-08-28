# AGD-0060: Asset Import

- **Scope:** Importing 3D models from glTF into the scene, including geometry, materials, and textures. Covers the vertex formats produced, tangent generation, and texture resolution. Does not cover scene file authoring or the rendering of what is imported.

## Overview

- Models are imported from glTF, in both its text and binary forms.
- Every imported material becomes a physically-based material. There is no other material path from glTF.
- Mesh primitives are flattened into a list of objects under a single root; the source file's node tree is not reproduced.
- Tangents are generated when the file does not supply them, so normal mapping works regardless of what an exporter chose to emit.
- Textures come from three places — embedded in the binary, referenced as external files, or inline as encoded data — and only the last is unsupported.

## Concepts

- **Primitive** — the smallest drawable unit in glTF: one set of vertices with one material. It maps to one object with one mesh component.
- **Flattening** — discarding the source file's node tree and producing one object per primitive under a single root.
- **Tangent basis** — the per-vertex vectors that let a normal map be interpreted in the surface's own orientation. Required for normal mapping.
- **Handedness** — a sign carried alongside each tangent recording which way the third basis vector points. Stored in the tangent's fourth component.

## Architecture

The importer parses a glTF file, walks its primitives, and for each one produces a scene object carrying a mesh component and a material.

Materials are constructed rather than referenced: base colour, metallic and roughness factors, and emissive colour are read from the file, and up to five texture maps are resolved and registered. Generated materials and textures are named after the source model and material index so that two models cannot collide.

Textures are registered centrally and checked for prior existence before loading, so a texture shared by several materials is loaded once.

Two vertex layouts are produced. A compact layout carries position, texture coordinate, and normal. A full layout adds a four-component tangent. Which one is produced depends on the entry point used, not on the file.

Imported objects reach a scene through the scene file's model field: the loader produces the flattened set, and they are reparented beneath the declaring object, whose transform becomes the model's root.

## Runtime flow

For each primitive in the file:

1. Read vertex positions, texture coordinates, and normals. A missing normal defaults to straight up rather than being recomputed from geometry.
2. Read tangents if present. If absent, generate them.
3. Convert indices to a single width regardless of how they were stored.
4. Build or reuse the material, resolving each texture map.
5. Create an object with a mesh component and attach the material.

**Tangent generation**, when needed, computes a direction per triangle from how texture coordinates change across it, accumulates that across every triangle touching a vertex, then orthogonalises the result against the vertex normal. Handedness is derived and stored with the tangent. The output matches the standard exporters produce, so generated and file-supplied tangents behave the same way.

**Texture resolution** follows the source. Data embedded in the binary is read directly from the buffer. An external reference is resolved relative to the model file's own location. Inline encoded data is detected, warned about, and skipped.

## Working with it

**Import a model into a scene.** Declare the model file on an object in the scene file. The imported objects are reparented beneath it and the declaring object's transform positions the whole model.

**Import a single mesh in code.** Use the single-mesh entry point and supply your own material. This produces the compact vertex layout, which suits shaders that do not read tangents.

**Import a full model in code.** Use the full entry point. This produces the tangent-carrying layout and generates physically-based materials, because that is what those materials need.

## Design decisions

### A single-header parser with no transitive dependencies

The parser is a single-header C library, chosen over both a heavier C++ glTF library and a general multi-format import library.

The deciding factor was integration cost against actual need. Only one format is required. A multi-format library brings hundreds of files and a build configuration to support formats nobody asked for, and the C++ alternative brings its own bundled image and JSON dependencies that duplicate what the project already has. The single-header option adds one file and imposes no build configuration, which also means it satisfies the project's static-runtime requirement without any special handling.

The cost is a lower-level interface: data is extracted through accessors by hand rather than arriving as ready-made objects.

### The node tree is flattened

One object is produced per primitive, all under one root. The source file's node hierarchy, and the transforms on it, are discarded.

The hierarchy exists in glTF principally to support animation and skinning, neither of which is supported here. Without those, preserving it would mean resolving nested transforms and carrying empty intermediate nodes for no observable benefit.

The cost is genuine and worth stating: spatial relationships between parts of a model are lost. A model whose parts are positioned by node transforms rather than by baked vertex positions will import wrongly. This is the decision to revisit first if animation is ever added, because animation cannot be built on a flattened import.

### Every imported material is physically-based

The importer targets one shader for all glTF materials, matching glTF's own core material model.

Supporting alternate material workflows would mean multiple shaders and a selection mechanism, for a workflow the format itself treats as an extension. Missing texture maps are handled by presence flags in the shader rather than by shader variants, so a material with only a base colour costs nothing extra.

The costs are that the alternative specular workflow is unsupported, none of the format's material extensions are honoured, and a model cannot request a different shader through its material data.

### Tangents are generated when absent, not required

Many exporters omit tangents. Rather than failing or silently producing broken normal mapping, the importer generates them using the standard method, and defers to the file's own tangents when they exist.

Preferring file-supplied data matters because an exporter knows things the importer does not, particularly around split vertices and mirrored geometry.

The fallback for a missing normal is different and weaker: it defaults to straight up rather than recomputing from geometry. A file without normals will shade incorrectly.

### Two vertex layouts, selected by entry point rather than by file

The compact layout omits tangent data; the full layout includes it. The choice follows which import entry point the caller used.

The reasoning is that the caller knows what their shader reads and the file does not. A caller supplying their own non-physically-based material would otherwise pay for tangent data that never reaches a shader.

The cost is that the two paths can diverge, and a caller who picks the compact path and then attaches a material needing tangents gets no diagnostic — just incorrect shading.

### Texture coordinates are flipped in the shader, not at import

Texture coordinate conventions differ between glTF and the renderer. The correction is applied in the shader, controlled by a constant, rather than by rewriting vertex data during import.

This keeps the importer a faithful pass-through of file data, which makes it far easier to reason about when something looks wrong. A load-time flip would bake the correction into the data, making it invisible and inconsistent if the same mesh were ever used with a shader that did not expect it.

The cost is that the correction lives away from the importer, so someone debugging texture orientation has to know to look at the shader.

## Limitations

- No animation of any kind — skeletal, morph target, or blend shape. Rigged models import as static geometry.
- Node transforms are discarded by flattening, so models relying on them import incorrectly.
- Only triangle geometry is imported; line and point primitives are skipped.
- Only the first texture coordinate channel is read. Vertex colours and skinning data are ignored.
- Only the metallic-roughness material workflow is supported, with no material extensions.
- Inline encoded textures are unsupported and skipped with a warning.
- Sparse accessor storage is unsupported.
- Missing normals default to straight up rather than being recomputed.
