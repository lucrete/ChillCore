# AGD-0120: UI Layout and Styling

- **Scope:** How interface screens are authored, laid out, styled, and drawn. Covers the markup and stylesheet subset, the layout algorithm, text rendering, batching, resolution scaling, and interaction. Does not cover navigation between screens, which is AGD-0130.

## Overview

- Screens are authored as real markup and stylesheet files. The same files open in a browser and run in the engine.
- Markup is parsed once into an engine-owned element tree. The parser's own representation is discarded immediately.
- Layout is a flexbox subset run in two passes: measure sizes bottom-up, then place elements top-down.
- Text uses distance-field glyph atlases generated offline, so any size is sharp from one texture.
- Drawing groups quads by texture, producing a handful of draw calls for a whole screen.
- All dimensions are authored against a fixed reference resolution and scaled at runtime.

## Concepts

- **Element** — one node in the engine's interface tree. Panels, buttons, text, sliders, toggles, and dropdowns are all elements.
- **Style state** — the visual variant an element is currently in: normal, hovered, pressed, or disabled. Each element carries a full style set per state.
- **Measure and arrange** — the two layout passes. Measure computes intrinsic sizes from the leaves upward; arrange distributes space from the root downward.
- **Reference resolution** — the authoring resolution against which all pixel values are written. Runtime scales from it.
- **Batch** — a group of quads sharing a texture, drawn together.
- **Action name** — an identifier in the markup connecting an interactive element to application code.

## Architecture

A parser reads the markup, walks it once to build the element tree, applies matching stylesheet rules, resolves inherited text properties, and then discards its own document representation. Nothing depends on the parser after loading.

A separate stylesheet parser handles selectors and property blocks. Its output is applied per element and per style state, so hover and pressed appearances are resolved at load rather than computed per frame.

The element tree is owned by the engine for the screen's lifetime. Elements hold their computed rectangle, their per-state styles, and a cached text drawing where applicable.

Layout runs over the tree. Text measurement is provided by the text renderer, so a text element's intrinsic size is its actual laid-out size rather than an estimate.

Drawing walks the tree, accumulating quads into batches grouped by texture. Solid colours use a single-pixel white texture with the colour in the vertices, so coloured and textured quads share one path and one shader.

Interaction is routed by name. Interactive elements carry an action identifier in the markup, which is mapped to application code at runtime. The markup contains no logic, and the application contains no layout.

## Runtime flow

**On load**, the markup and stylesheet are read, the element tree is built and styled, and layout is computed once.

**Per frame**, if nothing has invalidated layout, no layout work happens at all. The tree is walked to accumulate quads, text draws come from per-element caches, and the batches are submitted.

**Layout, when it does run**, is two passes.

1. **Measure, bottom-up.** Leaves compute intrinsic size — text by measuring, widgets from minimums. Automatic sizing resolves to content size, percentages to a fraction of the parent, and explicit values to their scaled amount. Minimum and maximum constraints apply, then padding is added.
2. **Arrange, top-down.** Free space along the main axis is computed and distributed according to the container's justification mode. Cross-axis alignment applies. Absolutely-positioned elements are placed from their parent's origin and excluded from flow. Each child then recurses with its computed rectangle as the constraint.

**Invalidation** is coarse. A window resize marks layout dirty and triggers a full recompute, which also invalidates every cached text drawing. Text caches invalidate individually when content, font, size, position, or alignment changes.

## Working with it

**Author a screen.** Write markup and a stylesheet. Keep the markup strictly well-formed — void elements self-closed, attributes quoted, every tag closed — because the parser is an XML parser. Open the file in a browser at the reference resolution to preview it.

**Wire up a button.** Give the element an action identifier in the markup and register a matching callback. The markup never names a function and the application never names a position.

**Style an interaction state.** Use the hover, active, and disabled pseudo-classes. They are resolved into per-state style sets at load, so there is no runtime cost to having them.

**Size something responsively.** Use percentages for proportional sizing and explicit values for fixed sizing. Explicit values are scaled from the reference resolution automatically; percentages resolve against the parent and are unaffected by scaling.

## Design decisions

### Screens are authored in real markup and stylesheets, kept browser-previewable

The decisive property is that the same files render in a browser and in the engine. Layout iteration happens with a file save and a browser refresh, with no build and no engine launch.

That is why the format is genuine markup rather than a similar-looking custom one. A custom format with the same parsing cost would lose the preview entirely, and would make authors learn something proprietary.

The requirement it imposes is strictness: files must be well-formed XML, because the parser is an XML parser. This costs a few authoring rules and buys a small, dependency-light parser. Browsers accept the stricter form without complaint, so the preview is unaffected.

Constructing interfaces in code was rejected for the same reason — no preview, and a rebuild for every layout change.

### The stylesheet subset is parsed by a purpose-built tokenizer

Roughly thirty properties are supported, with simple selectors and a few pseudo-classes. The tokenizer handles exactly that.

A full stylesheet parsing library would be a large dependency implementing a specification of enormous scope, almost none of which is used. Selector-plus-property-block syntax is small enough to parse directly.

The subset boundaries are deliberate. Only text properties inherit, matching the specification and preventing surprising layout effects from cascading. Selectors are simple — no compound selectors, no descendant combinators — which keeps matching a direct comparison rather than a tree query.

The cost is that unsupported syntax is silently ignored rather than reported, so a stylesheet that previews correctly in a browser can quietly differ in the engine.

### Layout is a flexbox subset, not a general layout engine

One algorithm covers the layouts actually needed: columns of menu items, rows of controls, centred content, spaced groups, and mixtures of fixed and flexible sizing.

Absolute positioning alone cannot express responsive layout — every element would need manual coordinates and nothing would survive a resize. A constraint solver is more general but adds substantial complexity and is harder to author than flex. Full layout as specified is an enormous surface with several interacting models.

Two passes are what flex layout requires: sizes must be known before space can be distributed, and space distribution determines final positions.

The cost is that layouts flex cannot express are not expressible, and there is no escape hatch other than absolute positioning.

### Text uses multi-channel distance fields from an offline atlas

Glyphs are stored as distance fields in an atlas generated ahead of time, so one texture serves every size sharply.

Bitmap fonts would need one atlas per size and blur when scaled. Runtime rasterisation would add a large dependency, per-glyph cost, and cache management. Single-channel distance fields round off sharp glyph corners, which is visible on characters with acute angles and unacceptable at small sizes; the multi-channel form preserves them.

Generating offline keeps the font library out of the runtime entirely. The cost is a build-time tool and generated artefacts in the repository, plus a fixed character set — anything outside it is unavailable, and adding characters means regenerating.

### Quads are batched by texture, and everything is a quad

Drawing accumulates quads into batches grouped by texture. Solid colours use a single-pixel white texture with colour supplied per vertex, so they batch alongside textured quads through one shader.

This collapses a screen to a handful of draw calls without any atlas packing or dirty-region tracking. Per-element draws would produce dozens per screen. A retained scene on the graphics side would require change tracking to avoid rebuilding, which is not worth it when a full rebuild is already only a few draw calls.

Nine-slice backgrounds fall out of the same mechanism as nine quads with computed coordinates, needing no special shader.

### Dimensions are authored at a reference resolution and scaled

Every pixel value is written as though the display were a fixed reference height, and multiplied by a single scale factor at runtime.

Designers work at a known size, and the browser preview at that size matches the engine exactly. Scaling is one multiplication per dimension.

The alternatives fail in specific ways: fixed pixels make the interface shrink as resolution rises; querying the operating system for display density is unreliable across multiple monitors and adds platform-specific code; percentage-only layout cannot express anything that must stay a fixed size.

The cost is that the scale factor derives from height alone, so unusual aspect ratios scale by height and may not fill or fit horizontally as intended.

### The tree is parsed once and never structurally changed

Loading builds the tree; unloading destroys it. There is no creation or removal of elements at runtime.

This makes ownership trivial and layout invalidation coarse — the tree is either valid or fully recomputed. Combined with per-element text caching, a screen that is not resizing costs nothing per frame in layout.

The cost is the obvious one: interfaces whose content varies — a list whose length depends on data — cannot be expressed by adding elements. They must be authored with sufficient elements and hidden, or the screen must be reloaded.

## Limitations

- No dynamic element creation. Data-driven lists must be pre-authored or the screen reloaded.
- Unsupported stylesheet syntax is ignored silently, so browser preview and engine output can diverge without warning.
- Only text properties inherit; everything else must be set per element.
- Selectors are simple only — no compound, descendant, or attribute selectors.
- The character set is fixed at atlas generation. Text outside it cannot be displayed.
- Resolution scaling follows height alone, so extreme aspect ratios are not handled specially.
- Layout invalidation is all-or-nothing. Any resize recomputes the whole tree and invalidates every text cache.
