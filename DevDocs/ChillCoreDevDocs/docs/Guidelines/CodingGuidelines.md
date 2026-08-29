# ChillCore Coding Guidelines
These are guidelines, not rules, where if followed will result in an easier to understand codebase.

## Simplicity and Clarity
* Elevate **Make it Easy to Understand** to the same priority as **Make it Work**.  
* "Write programs for people first, computers second."

## Guidelines: 
* Single Responsibility Principle
* DRY: Don't Repeat Yourself
* Orthogonality (no coupling)  

# C++ Style Guide

## Naming Conventions
* Use descriptive names to write self-commenting-code.
* Avoid abbreviated words that truncate or remove letters, as they often only make sense to the creator at time of writing. Commonly used english-language abbreviations are acceptable, like Ms (milliseconds), fps (frames per second)
* Single-letter variables for tight loops.
* Prefer prefixing bools with a verb: isActive, doNotify.
* Only use underscore prefix for function parameters for which a member or local variable of the same name already exists. See many of the Init() functions.
* No hungarian notation

## Code Comments
* Only add comments where extra explanation is required. 
* Prefer self-commenting-code through descriptive naming. 
* Use comments to provide context and justification.
* Avoid Todo comments. Capture future work in a dev issue or tech backlog to surface it for project management. Use Todo only for work-in-progress that spans multiple Pull Requests.
* Do not add xml code instrumentation comments unless the intent of function is not obvious.
* Use Comment header blocks to organize code and assist in visual scanning:
~~~csharp
    // ========================
    // Section
    // ========================
~~~

## Casing terminology
* camelCase: no spaces, capitalize first letter only of each word except the first word. Including acronyms. Variables, function parameters.
* PascalCase: capitalize first letter of all words including the first. All file and folder names, whether checked in or created by the application at runtime. Classes, structs, functions, properties, enum members.
* SCREAMING_SNAKE_CASE: for macros, pre-processor defines, header guards, and const variables.
* Do not use kebab-case.

## Braces
* Allman style braces, with the first brace on the new line, as opposed to K&R with the first brace on the same line. 
* Always use braces even for one-line bodies.
~~~
if (isActive)
{
    TakeAction()
}
~~~

### Compact form for table-aligned parallel guards
A narrow exception to the 4-line Allman rule. A single-statement guarded call may be written on one line as `if (condition) { statement; }` **only when all of the following hold**:

1. **Grouped.** It appears as part of a contiguous run of **2 or more** adjacent lines with the same structural shape `if (cond) { call; }`. An isolated guard still uses the full 4-line form.
2. **Single simple statement.** The body is one expression statement — no locals, no nested control flow, no `else`.
3. **Aligned as a table.** Lines in the group are whitespace-aligned so the parallel structure reads as a vertical scan, and every line fits within the column limit without wrapping.
4. **Homogeneous group.** Every line in the group uses the compact form. If any entry's body grows beyond a single statement, expand the whole group to full Allman.

The value is scan speed on parallel structure — a table-aligned block communicates "these N things are the same operation with varying inputs" faster than N four-line blocks. That benefit only exists when the criteria above hold; violate any one of them and the expanded form is better.

**Permitted:**
~~~cpp
if (mrHandle.IsValid())        { gfxApi->BindTexture(1, mrHandle,        samplerHandle); }
if (normalHandle.IsValid())    { gfxApi->BindTexture(2, normalHandle,    samplerHandle); }
if (occlusionHandle.IsValid()) { gfxApi->BindTexture(3, occlusionHandle, samplerHandle); }
if (emissiveHandle.IsValid())  { gfxApi->BindTexture(4, emissiveHandle,  samplerHandle); }
~~~

**Disallowed (isolated guard):**
~~~cpp
if (texture != nullptr) { DoWork(texture); }
~~~

**Disallowed (mixed forms in one group):**
~~~cpp
if (a.IsValid()) { DoA(a); }
if (b.IsValid())
{
    DoB(b);
    MaybeLog(b);
}
~~~

## Namespaces
- **CC::** Core engine functionality (rendering, input, math, scene management)
- **No namespace:** Application-specific code in App folder

## Regions
Avoid using regions in files.

---

# C++ Patterns and Conventions

## Include Guards
Use `#ifndef` style include guards (not `#pragma once`). Guard name matches filename in SCREAMING_CASE with `_H` suffix.
~~~cpp
#ifndef CLASSNAME_H
#define CLASSNAME_H
// ...
#endif
~~~

## Header File Organization
Order sections as follows:

1. Include guards
2. Standard library includes (`#include <...>`)
3. Project includes (`#include "..."`)
4. Namespace declaration
5. Forward declarations (if needed)
6. Class definition
7. Closing namespace and `#endif`

## Class Member Organization
Within a class, order access sections:

1. `public:` - constructors, destructor, static accessors, public methods, public members
2. `protected:` - members needed by derived classes
3. `private:` - implementation details, helper methods, member variables

## .cpp File Organization
1. Main header include (the .h file it implements)
2. Standard library includes
3. Project includes
4. Namespace declaration
5. Static member initialization
6. Constructor, destructor
7. Static accessor (Get/Instance)
8. Public methods (same order as header)
9. Private helper methods

## Singleton Pattern
Managers use a consistent singleton pattern:
~~~cpp
// In .h
class Manager
{
public:
    static Manager* Get();  // or Instance()
private:
    static Manager* instance;
};

// In .cpp
Manager* Manager::instance = nullptr;

Manager::Manager()
{
    CC_ASSERT(instance == nullptr, "Manager already created");
    instance = this;
}

Manager::~Manager()
{
    instance = nullptr;
}

Manager* Manager::Get()
{
    CC_ASSERT(instance != nullptr, "Manager not created yet");
    return instance;
}
~~~

## Constructor Initialization Lists
* One member per line, comma at start of line, aligned.
~~~cpp
ClassName::ClassName()
    : memberOne(0)
    , memberTwo(nullptr)
    , memberThree(false)
{
~~~

## Parameter Passing Conventions
* **Const reference**: For complex types (strings, vectors, matrices): `const std::string&`, `const CCVector3&`
* **By value**: For primitives (int, float, bool)
* **Raw pointer**: For ownership transfer or optional parameters
* **Reference out-parameters**: For multiple return values: `void GetSize(int& width, int& height)`

## String Handling
* Prefer `std::string` for string storage and manipulation.
* Use `const char*` only for legacy C-style interfaces or static constants.

## Container Selection
* Prefer raw arrays with `static const int MAX_X` when possible
* Prefer integer-based (includes enums) indices and keys over strings when possible
* `std::map<std::string, T>` - named resource lookups where order matters
* `std::unordered_map<std::string, T>` - performance-critical lookups
* `std::vector<T>` - dynamic arrays
* `std::array<T, N>` - fixed-size arrays

## Smart Pointer Usage
* `std::unique_ptr<T>` - for owned resources in containers (maps)
* Raw `new`/`delete` - for singleton subsystems (manager owns lifecycle)
* Cleanup in destructor should be reverse order of construction

## Enum Conventions
* Use `enum class` for standalone type-safe enums.
* Use unscoped `enum` inside a `namespace` for grouped constants that need implicit conversion:
~~~cpp
namespace InputTrigger
{
    enum Trigger
    {
        DpadUp,
        DpadDown,
        TriggerMax  // Sentinel value for array bounds
    };
}
~~~
* Include a `Max` or `Count` sentinel value when enum is used for array sizing.

## Const-Correctness
* Mark getter methods `const`.
* Return const references for expensive-to-copy members: `const CCVector3& GetPosition() const`
* Return by value for computed results or small types.
* Provide both const and non-const overloads for Transform-style accessors:
~~~cpp
Transform& GetTransform();
const Transform& GetTransform() const;
~~~

## Accessor Naming
* Getters: `GetPropertyName()` or `PropertyName()` for simple inline getters
* Setters: `SetPropertyName(value)`
* Boolean queries: `IsPropertyName()` or `HasPropertyName()`
* Toggle methods: `TogglePropertyName()`

## Virtual Functions
* Always declare destructor `virtual` in base classes.
* Use `virtual` keyword in derived class declarations.
* Use `override` keyword for overridden methods.
* Use `= 0` for pure virtual (abstract) methods.

## Static Constants
* Prefer `static constexpr` for compile-time constants:
~~~cpp
static constexpr int MAX_COUNT = 1024;
static constexpr float DEADZONE = 0.15f;
~~~
* Use `static const char*` for string constants.

## Default Member Initialization
* Initialize members at declaration when a sensible default exists:
~~~cpp
bool isActive = false;
Texture* texture = nullptr;
~~~

## Deleted Constructors
* Explicitly delete default constructor when parameters are required:
~~~cpp
ClassName() = delete;
~~~

## Struct vs Class
* Use `struct` for plain data containers with public members (vertices, faces, definitions).
* Use `class` for types with behavior, invariants, or encapsulation needs.

## Error Handling
* Use `CC_ASSERT(condition, message)` for debug-time invariant checking.
* Use try-catch with graceful degradation for recoverable errors (file loading).
* Provide fallback defaults (error shader, default texture) when resources fail to load.

## Function Returns
* Single Exit, one return per function.
  * Anti-pattern: returns in the middle of a function, making the side effects of the function differ depending on whether the exit is hit or not.

## File I/O
All file reads and writes go through `CC::PlatformFileSystem::Get()` to support cross-platform file io. Do not use direct file io function calls.

---



