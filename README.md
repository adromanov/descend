# Descend

A C++ library for composable, type-safe algorithm pipelines.

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Status](https://img.shields.io/badge/status-work--in--progress-yellow.svg)](https://github.com)

## Overview

Implementation is based on Google's [Rappel framework talk](https://www.youtube.com/watch?v=itnyR9j8y6E).

Descend provides a functional interface for building type-safe data processing pipelines through composable stages. It supports:

- **Element-wise and whole-collection processing** - Incremental and complete modes
- **Multi-argument pipelines** - Pass multiple values through stages with `args<...>`
- **Higher-order operations** - Group, branch, and aggregate data streams
- **Compile-time composition** - All stage connections resolved at compile time
- **Reference semantics preservation** - Maintains const&, &, && through pipelines
- **Custom generators** - Infinite and finite sequences
- **Debug visualization** - Inspect pipeline structure and types

> **Note on AI Usage**: [Claude Code](https://www.anthropic.com/claude/code) was used to write approximately 10% of this project, including documentation, CMake files, some tests, comments, and simple stages. All AI-generated code was carefully reviewed, no "vibe-coding" allowed.

## Quick Example

```cpp
#include "descend/descend.hpp"
namespace dd = descend;

// Simple pipeline
std::vector<int> numbers = {1, 2, 3, 4, 5};
dd::apply(
    numbers,
    dd::filter([](int x) { return x % 2 == 0; }),
    dd::transform([](int x) { return x * 2; }),
    dd::for_each([](int x) { std::cout << x << '\n'; })
);
// Output: 4, 8

// Generator-based pipeline with enumerate
auto result = dd::apply(
    dd::iota(1),  // infinite sequence
    dd::filter([](int x) { return x % 3 == 0; }),
    dd::take_n(5),
    dd::enumerate(),  // Add index to each element
    dd::transform([](int idx, int val) { return val * 2; }),
    dd::to<std::vector>()
);
// result: {6, 12, 18, 24, 30}

// Group and aggregate
dd::apply(
    std::vector{1, 2, 3, 4, 5, 6, 7, 8},
    dd::map_group_by<std::map>(
        [](int x) { return x % 3; },  // Group by remainder
        dd::count()
    ),
    dd::for_each([](int key, size_t count) {
        std::cout << "Remainder " << key << ": " << count << " items\n";
    })
);
```

## Core Stages

### Incremental → Incremental (element-wise processing)
- `transform(f)` - Map elements through function
- `filter(pred)` - Keep elements matching predicate
- `take_n(n)` - Take first n elements
- `enumerate<Index>(start = {})` - Prepend incrementing index to each element
- `expand()` - Expand tuples/pairs into multiple arguments
- `zip_result(f)` - Append function result to input arguments
- `swizzle<I...>()` - Reorder arguments by index
- `flatten()` - Flatten last range-like argument and passes it further together with other arguments (safe: converts T&& → const T&)
- `flatten_forward()` - Same as above, but preserves T&&
- `transform_arg<I>(f)` - Transform specific argument in args tuple
- `unwrap_optional()` - Unwrap optional values or short-circuit on nullopt
- `make_pair()` - Convert two-argument input (`args<A, B>`) to `std::pair<A, B>`
- `make_tuple()` - Convert multi-argument input (`args<...>`) to `std::tuple<...>`
- `construct<T>()` - Construct object of type `T` from pipeline arguments

### Incremental → Complete (reducing operations)
- `min()` - Find minimum element (returns `optional<T>`)
- `max()` - Find maximum element (returns `optional<T>`)
- `min_max()` - Find both minimum and maximum (returns `optional<struct {T min, max;}>`)
- `count()` - Count elements (returns `size_t`)
- `accumulate(init, op)` - Reduce with binary operation
- `to<Container>()` - Collect elements into container
- `for_each(f)` - Apply side effect to each element (terminal)

### Complete → Complete (whole-input operations)
- `sort(comp = std::less<>)` - Sort collection in-place (use `std::ref()` for lvalues)
- `stable_sort(comp = std::less<>)` - Stable sort in-place (use `std::ref()` for lvalues)
- `transform_complete(f)` - Transform entire input at once
- `expand_complete()` - Expand tuples into separate arguments
- `unwrap_optional_complete()` - Unwrap optional or short-circuit on nullopt

**Note on `flatten()` vs `flatten_forward()`**: `flatten()` converts rvalue references to const lvalue references for prefix arguments, preventing accidental moves during iteration. `flatten_forward()` preserves rvalue references, placing responsibility on the developer to ensure arguments are not used after being moved from.

## Higher-Order Stages

- `tee(subchain1, subchain2, ...)` - Process each element through multiple independent pipelines, collect results as tuple
- `map_group_by<Map>(key_getter, stages...)` - Group all elements by key globally, process each group through a pipeline
- `group_by(key_getter, stages...)` - Group consecutive elements with same key, emit groups as they complete (streaming)

## Generators

- `iota(start)` - Infinite sequence from start
- `iota(start, end)` - Bounded sequence [start, end)
- `generator<T>(f)` - Custom generator from lambda

## Processing Modes

Stages can process data in two ways:

- **Incremental (element-wise)**: `process_incremental(element, next)` - handles one element at a time
- **Complete (whole collection)**: `process_complete(collection, next)` - operates on entire input

The chain component automatically converts complete inputs to incremental processing by iterating and calling `process_incremental` on each element. This allows seamless composition complete output stages with incremental input stages.

## Advanced Features

### Multi-Argument Pipelines

`args<...>` is used to pass multiple values through stages:

```cpp
dd::apply(
    vec,
    dd::enumerate(),  // Produces args<const int&, const T&>
    dd::zip_result([](int idx, T val) { return val * 2; }),  // args<const int&, const T&, T>
    dd::for_each([](int idx, T orig, T doubled) { ... })
);
```

### Higher-Order Processing

```cpp
// Process data through multiple pipelines simultaneously
dd::apply(
    data,
    dd::tee(
        dd::compose(dd::filter(pred1), dd::count()),
        dd::compose(dd::filter(pred2), dd::to<std::vector>())
    ),
    dd::expand_complete(),
    dd::for_each([](size_t count, std::vector<T> filtered) { ... })
);

// Group and aggregate
dd::apply(
    records,
    dd::map_group_by<std::map>(
        [](auto& r) { return r.category; },
        dd::transform([](auto& r) { return r.value; }),
        dd::accumulate(0.0, std::plus<>{})
    ),
    dd::for_each([](auto category, double sum) {
        std::cout << category << ": " << sum << '\n';
    })
);
```

### Debugging Pipelines

Use `apply_debug()` to visualize chain structure:

```cpp
#include "descend/debug.hpp"

auto result = dd::apply_debug(
    data,
    dd::transform(...),
    dd::filter(...),
    dd::to<std::vector>()
);
// Prints: stage types, input/output types, processing modes
```

### Other Features

- **Compositions**: Use `compose()` to merge inputs and stages or several stages.
- **Reference semantics**: Preserves const&, &, && through pipelines
  - **Input forwarding**: `vec` → `const T&`, `std::ref(vec)` → `T&`, `std::move(vec)` → `T&&`
  - **Generators**: Produce whatever their `output_type` specifies
  - **Return value safety**: Value semantics by default (like `std::make_pair`/`std::make_tuple`)
    - All references in `args<>`, `tuple<>`, `pair<>` are converted to values
    - Opt-in for references using `std::reference_wrapper<T>`
- **Type safety**: Compile-time type checking of stage composition
- **Automatic conversion**: Complete → Incremental handled transparently by chain
- **Complex iterations**: Supports nested loops (see Pythagorean triples example in main.cpp)
- **Custom generators**: Create infinite or finite sequences with `generator<T>(f)`

## Building

```bash
mkdir build && cd build
cmake ..
make
./descend_example
```

### Build Types

**Release** (default, optimized):
```bash
cmake -DCMAKE_BUILD_TYPE=Release ..
```

**Debug** (with sanitizers enabled):
```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..
```

**RelWithDebInfo** (optimized with debug symbols):
```bash
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
```

**MinSizeRel** (size-optimized):
```bash
cmake -DCMAKE_BUILD_TYPE=MinSizeRel ..
```

To disable sanitizers in Debug builds:
```bash
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_SANITIZERS=OFF ..
```

## Creating Custom Stages

Stages implement a simple protocol:

```cpp
struct my_stage
{
    static constexpr auto style = stage_styles::incremental_to_incremental;

    template <class Input>
    static constexpr auto do_something_with_input(Input&& input)
    {/* ... */}

    template <class Input>
    struct impl
    {
        using input_type = Input;
        using output_type = decltype(do_something_with_input(std::declval<Input>()));
        using stage_type = my_stage;

        template <class Next>
        constexpr void process_incremental(Input&& input, Next&& next)
        {
            // Process and forward to the next stage
            next.process_incremental(do_something_with_input((Input&&) input));
        }
    };

    template <class Input>
    constexpr auto make_impl() { return impl<Input>{}; }
};
constexpr auto my()
{
    return my_stage{};
}
```

**Important**: If your stage carries data (functors, state), provide both lvalue and rvalue `make_impl()` overloads for efficiency in higher-order stages:

```cpp
template <class Input> auto make_impl() &  { return impl<Input>{data}; }
template <class Input> auto make_impl() && { return impl<Input>{std::move(data)}; }
```

See `stages.hpp` for complete examples.

## File Structure

- `descend/descend.hpp` - Main header with all core stages
- `descend/apply.hpp` - provides `apply()` function - main entry point to build and run the computation
- `descend/higher_order.hpp` - Higher-order stages (tee, map_group_by) - included by descend.hpp
- `descend/debug.hpp` - Debug utilities (optional, include separately for `apply_debug`)

## Requirements

- C++20 or later

## More Examples

### Run-length encoding with `group_by`

```cpp
// group_by groups consecutive elements - perfect for run-length encoding
auto rle = dd::apply(
    std::string_view{"aaabbaac"},
    dd::group_by(
        std::identity(),  // Key = the character itself
        dd::count()       // Count consecutive occurrences
    ),
    dd::make_pair(),      // Convert args<char, size_t> to pair
    dd::to<std::vector>()
);
// rle: [('a', 3), ('b', 2), ('a', 2), ('c', 1)]
// Note: 'a' appears twice because group_by tracks consecutive runs
```

### Finding statistics with `min_max`

```cpp
struct Measurement { std::string sensor; double value; };
std::vector<Measurement> data = {{"A", 23.5}, {"B", 18.2}, {"A", 25.1}, {"B", 17.8}};

auto stats = dd::apply(
    data,
    dd::transform(&Measurement::value),
    dd::min_max()
);
// stats: optional<{min: 17.8, max: 25.1}>

if (stats) {
    std::cout << "Range: " << stats->min << " to " << stats->max << '\n';
}
```

### Constructing objects from pipeline data

```cpp
struct Point { int x, y; };

auto points = dd::apply(
    dd::iota(0, 5),
    dd::zip_result([](int i) { return i * i; }),  // args<int, int>
    dd::construct<Point>(),                        // Construct Point from args
    dd::to<std::vector>()
);
// points: [{0,0}, {1,1}, {2,4}, {3,9}, {4,16}]
```

### Difference between `group_by` and `map_group_by`

```cpp
// group_by: consecutive grouping (streaming, emits as groups complete)
dd::apply(
    std::vector{1, 1, 2, 2, 1, 1},
    dd::group_by(std::identity(), dd::to<std::vector>()),
    dd::make_pair(),
    dd::for_each([](int key, auto vec) {
        std::cout << key << ": " << vec.size() << " items\n";
    })
);
// Output: 1: 2 items, 2: 2 items, 1: 2 items (three groups!)

// map_group_by: global grouping (collects all, then emits)
dd::apply(
    std::vector{1, 1, 2, 2, 1, 1},
    dd::map_group_by<std::map>(std::identity(), dd::to<std::vector>()),
    dd::for_each([](int key, auto vec) {
        std::cout << key << ": " << vec.size() << " items\n";
    })
);
// Output: 1: 4 items, 2: 2 items (two groups - all 1s combined)
```

### Branching computation with tee

```cpp
auto result = dd::apply(
    dd::iota(1, 100),
    dd::tee(
        dd::compose(dd::filter([](int x) { return x % 2 == 0; }), dd::count()),
        dd::compose(dd::filter([](int x) { return x % 3 == 0; }), dd::count()),
        dd::max()
    ),
    dd::expand_complete(),
    dd::transform_complete([](size_t evens, size_t threes, std::optional<int> max) {
        return std::tuple{evens, threes, max.value_or(0)};
    })
);
// result: std::tuple{49, 33, 99}
```
