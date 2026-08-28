# AGD-0050: Scene Hierarchy

- **Scope:** The scene object model — the tree, transforms, components, and how a scene is loaded from file. Covers ownership, lifecycle propagation, and the component registration mechanism. Does not cover rendering or individual component behaviours.

## Overview

- A scene is a tree of named objects. Each object has a transform, a list of components, and optional children.
- Behaviour is attached, not inherited. Rendering is a component like any other, so an object with no visual representation is a normal object rather than a special case.
- Lifecycle calls propagate down the tree: the hierarchy tells objects, objects tell their components and then their children.
- Scenes are authored in YAML. New component types become available to scene files without editing any central list.
- Ownership follows the tree exactly. Destroying an object destroys its children and components.

## Concepts

- **Scene object** — a named node in the tree. Always has a transform; may have components and children.
- **Component** — an attachable behaviour owned by a scene object, reaching its object to read or modify the transform and to find sibling components.
- **Transform** — position, rotation, and scale in the object's own space. World space is derived by walking up the parent chain.
- **Component factory** — the registry that maps a type name in a scene file to the code that constructs it.

## Architecture

`SceneHierarchy` is a singleton owning the root objects, propagating lifecycle calls, and holding the enabled and paused flags that govern whether updates run at all.

`SceneObject` owns its transform as a direct member, its components, and its children. It provides lookup of a component by type, and computes its own world matrix on demand.

`Component` is the base for attached behaviour. It knows its owning object, whether it is enabled, and whether it responds to pause. Rendering, lights, and gameplay behaviours are all components; none of them subclass `SceneObject`.

`ComponentFactory` maps type-name strings to construction functions. Registration is by a static object in each component's own translation unit, so the factory has no list of known types and no central file records them.

`SceneLoader` reads a scene file and builds the tree. `Transform` holds rotation as a quaternion internally while accepting and reporting Euler angles at its boundary.

## Runtime flow

**Loading** proceeds in a fixed order.

1. Parse the scene file.
2. Create all materials. They come first because components reference them by name, so they must exist before any component is constructed.
3. Create objects depth-first, constructing each component through the factory and letting it read its own properties from the file.
4. Load any declared models, reparenting their generated objects under the declaring object.
5. Assemble parent-child links.

**Per frame**, the hierarchy updates its root objects, and each object updates its components before its children. Renderables submit themselves to the render list during this pass, which is why a scene that stops updating also stops being drawn.

**World transforms** are computed on request by walking up the parent chain and combining each level. Nothing is cached and no dirty flag exists.

**Shutdown** propagates children-first, so a child can rely on its parent still existing while it tears down.

## Working with it

**Add a component type.** Derive from the component base, implement whichever lifecycle methods the behaviour needs, and add a static registrar in the implementation file mapping a type name to a construction function. The type name becomes usable in scene files immediately, with no central registration to update.

**Author a scene.** A scene file has two top-level sections. Materials are declared first, then objects as a recursive tree, each naming its transform, its components by registered type name, and its children.

**Load a model.** An object may declare a model file instead of, or alongside, components. The loaded meshes are reparented beneath the declaring object, whose transform becomes the model's root.

**Mix loaded and constructed objects.** Objects created in code can be added to the hierarchy alongside file-loaded ones. Nothing distinguishes them afterwards.

## Design decisions

### Composition, not inheritance, for object behaviour

An object is a node with a list of behaviours rather than a base class to specialise. The alternative — subclassing the scene object for each kind of thing — was rejected because it forces a single-inheritance taxonomy onto objects that naturally have several independent traits.

The concrete payoff is that rendering is not privileged. A renderable is a component, so objects without visual representation — empty parents, logical groupings, markers — exist without a special case, and an object can gain or lose visibility by attaching or detaching a component.

The cost is indirection when finding behaviour: locating a component by type is a scan and a type test rather than a direct member access.

### Every object has a transform, as a member rather than a component

Spatial position is treated as intrinsic to being in a scene, not as an optional attached behaviour.

Making the transform a component would mean every access checks for its presence, and every object would have one anyway. The parent-child relationship is fundamentally spatial, so the tree would be meaningless without it.

The cost is that a purely logical object still carries transform data it never uses. That is a few bytes against a great deal of removed checking.

### Rotation is stored as quaternions and exposed as Euler angles

Internally rotation is a quaternion, avoiding the gimbal lock and interpolation problems Euler angles bring. Externally the interface takes and returns degrees, because that is what a human authoring a scene file or reading a value in a debugger wants.

The conversion happens at the boundary. The cost is that a round trip through the Euler interface is not guaranteed to return identical values, since several Euler triples describe the same orientation.

### World matrices are recomputed on demand, never cached

Asking for a world matrix walks up the parent chain and combines transforms at each level. There is no caching and no dirty-flag propagation.

This is a deliberate bet on scene scale: hierarchies are shallow and object counts are low, so the walk is cheap and a caching scheme would cost more in complexity and invalidation bugs than it saves. The alternative — dirty flags propagating down subtrees — is the standard answer at larger scales and is where this should go if scenes grow.

The cost is that world-matrix access is not free and repeated queries in one frame repeat the work. Anything querying in a tight loop should hoist the result.

### Component types self-register from their own translation unit

Adding a component type requires touching only that type's own files. A static registrar in the implementation file adds it to the factory before anything runs.

The alternative — a central switch or registration list — makes every new type a change to a shared file, with the accompanying merge conflicts, and creates a second place to forget.

The costs are the usual ones for this pattern. Registration order across translation units is unspecified, so registrars must not depend on each other. A type whose implementation file is not linked in silently does not exist, and the failure appears as an unknown type at scene-load time rather than as a link error.

### Materials are loaded before objects

Scene loading creates every material before constructing any object, because components reference materials by name and would otherwise be constructed against something that does not exist.

The alternative — resolving references lazily after loading — would allow either order but introduces a window where a component holds an unresolved name, and requires a second pass. A fixed order removes the question.

The cost is that materials cannot reference objects, which is not a limitation anyone has wanted.

### Ownership follows the tree

The hierarchy owns roots, objects own their children and components, and destruction cascades. There is no shared ownership and no reference counting.

The tree already describes the lifetime relationships, so making ownership follow it means there is one structure to reason about instead of two. Shutdown running children-first means a child tearing down can still reach its parent.

The cost is that objects cannot be shared between parents, and any long-lived reference to an object held elsewhere becomes dangling when the tree is destroyed. Nothing enforces this.

## Limitations

- No caching of world transforms. Repeated queries in a frame repeat the walk, and deep hierarchies multiply the cost.
- Component lookup by type is a linear scan with a runtime type test.
- Component registration order across translation units is unspecified, and a component whose implementation is not linked in fails at scene-load time rather than at build time.
- Objects cannot be shared between parents, and external references are not tracked, so holding a pointer across a scene teardown is unsafe.
- Disabling the hierarchy stops renderables submitting themselves, so the scene disappears rather than freezing. Freezing is what the pause flag is for.
