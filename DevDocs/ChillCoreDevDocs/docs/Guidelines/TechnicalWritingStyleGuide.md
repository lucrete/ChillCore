# Technical-writing Style Guide
## Engineering-centric
* Intended for future maintainers skimming for intent.
* State *what exists* and *why it exists*, then move on.

Characteristics:

* Short declarative sentences
* Minimal rhetorical framing
* Almost no transitional prose
* Avoiding over-specification

### Bulleted structure over narrative flow

Prefer:

* Lists over paragraphs
* Indentation to imply hierarchy
* Headings that act as anchors, not section introductions

For example:
* list component responsibilities rather than summarizing them
* avoid wrapping lists in explanatory paragraphs unless necessary

Docs should be written so they are **scannable first, readable second**.

### An AGD does not reference another AGD

Architectural Guide Docs stand alone. An AGD must not cite another AGD, by number or by title.

Numbers get renumbered, guides get merged and retired, and nothing verifies a reference in prose. A stale pointer sends a reader somewhere wrong with full confidence.

Where a boundary matters, state it — "does not cover the graphics abstraction beneath it" — rather than naming the guide on the other side. Where a decision in another guide matters, restate the part that matters in one sentence. Duplicating a sentence is cheaper than a dangling reference.

This applies to AGDs only:

* **Planning docs reference each other and AGDs freely.** They track work in motion, and the relationship between a plan and the guide it will become is the point.
* **`index.md` links to the guides.** Navigating to them is its job, and its markdown links fail loudly rather than silently.
* **Code comments cite nothing** — see the Coding Guidelines.

### Minimal emphasis language
Keep the tone neutral and technical.
Emphasis comes from:

* Placement
* Section headers
* List order
