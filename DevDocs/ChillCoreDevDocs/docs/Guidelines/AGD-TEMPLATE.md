# AGD-TEMPLATE

Template and authoring rules for Architectural Guide Docs. Copy the structure below into `Architecture/AGD-NNNN-SystemName.md` and replace the guidance under each heading with real content.

## What an AGD is

An Architectural Guide Doc is the single document for one engine system. It serves two readers, in this order:

1. A developer meeting the system for the first time, who needs to understand it well enough to work in it.
2. A developer asking why it is built this way, who needs the reasoning behind the current shape.

It describes the system as it is now, and is updated whenever the architecture changes. It is not a decision log and not a specification.

## Authoring rules

* Code is the source of truth. Never duplicate it.
  * Snippets are permitted as short illustrations of usage.
  * Never lift implementation, and never transcribe structs, enums, or signatures.
* Describe behaviour and responsibilities. Name types and classes freely — they are stable and greppable.
* No file paths or directory listings by default. Include a path only when the layout itself is the thing being explained.
* No dates, and no change log. Git owns both.
* Decisions are unnumbered. State the decision and the reasoning.
* Verify every claim against the current source code before writing it. Nothing inside an AGD can signal that it has gone stale, so accuracy depends entirely on this step.
* Apply `TechnicalWritingStyleGuide.md`. Scannable first, readable second.

## Scale

* Required sections: Overview, Architecture, Design decisions, Limitations.
* Optional sections: Concepts, Runtime flow, Working with it.
* A small system uses the required four. Do not pad.
* Delete every unused optional section, and delete this guidance from the copy.

---

# AGD-NNNN: [System name]

- **Scope:** One sentence. What this system covers, and what it deliberately does not.

## Overview

Three to six bullets. What the system does, where it sits in the engine, what problem it exists to solve. A reader should know from this section alone whether this is the document they need.

## Concepts

Optional. Vocabulary and mental model the reader needs before the rest parses.

* Include only terms this system defines, or uses in a specific way.
* Skip the section entirely when the system introduces no new vocabulary.

## Architecture

The components, their responsibilities, and who owns what.

* Name the types.
* Describe the boundaries between them, and why those boundaries are where they are.
* A mermaid diagram belongs here when the relationships are hard to hold in prose.

## Runtime flow

What actually happens, in order — per frame, per event, or per lifecycle transition as fits the system. This is the section that turns a list of components into an understanding of how they cooperate.

## Working with it

Optional, and high value where it applies. Task-oriented recipes for what a developer will actually need to do.

* One sub-heading per task. "Add a new action." "Make a material transparent."
* Short illustrative snippets belong here, not implementation excerpts.

## Design decisions

The reasoning behind the current shape. One sub-heading per decision, unnumbered, stated as a claim rather than a topic — "Transparent objects are sorted per object, not per triangle", not "Sorting".

Each decision covers:

* What was decided.
* Why, including what it was weighed against.
* What it costs.

Record superseded reasoning in place only where it explains the current design: state what changed and why. Do not maintain a history for its own sake.

## Limitations

What does not work, stated plainly. Known artefacts, unsupported cases, scale ceilings.

This is the section that saves the most reader time. Be specific, and do not soften.
