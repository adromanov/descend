# Descend - Technical Context

## Architecture Overview

Descend is a C++ library implementing composable algorithm pipelines through type-level stage composition.

## Core Components

### Chain System (`chain.hpp`)
- `stage_chain_component<Chain, I, Stage>` - CRTP-based stage wrapper
- `chain<IndexSequence, Stages...>` - Variadic inheritance of stage components
- Each stage knows its input/output types and can route to next stage
- Two processing modes: `process_incremental` (element-wise) and `process_complete` (whole collection)
- Chain component automatically converts Complete → Incremental by iterating input

**Stage Processing Styles** (`stage_styles.hpp`):
Every stage declares a `style` member of type `stage_style` with two fields:
- `input` - Whether the stage accepts incremental or complete input
- `output` - Whether the stage produces incremental or complete output

Three predefined styles:
- `incremental_to_incremental` - Element-wise processing (e.g., `transform`, `filter`)
- `incremental_to_complete` - Reducing operations (e.g., `count`, `max`, `to`)
- `complete_to_complete` - Whole-collection operations (e.g., `sort`, `transform_complete`)

The chain component uses stage styles to determine which processing method to call and validate stage connections at compile time.

### Stage Types (`stages.hpp`)
All stages implement the protocol:
- `input_type` / `output_type` type members
- `make_impl<In>()` - Creates type-specific implementation
- `process_incremental(in, next)` - Process one element, forward to next stage (only for stages with incremental input)
- `process_complete(in, next)` - Process entire collection at once (only for stages with complete input)
- Optional: `end(next)` - Finalize and forward result (for reducing stages)

**Stage Data Handling**:
When a stage carries data (functors, initial values, etc.), it must handle reusability in higher-order stages like `map_group_by`, which may create chains from stages multiple times:

1. **Option 1**: Provide both lvalue and rvalue `make_impl()` overloads:
   ```cpp
   template <class Input> auto make_impl() &;   // Copy data
   template <class Input> auto make_impl() &&;  // Move data
   ```

2. **Option 2**: Always copy data in `make_impl()`:
   ```cpp
   template <class Input>
   constexpr auto make_impl() {
       return impl<Input>{f};  // Copy f each time
   }
   ```

**Stage Transitions**:
- **Incremental → Incremental**: `transform`, `filter`, `take_n`, `expand`, `swizzle`, `flatten`, `flatten_forward`, `enumerate`, `zip_result`, `transform_arg`, `unwrap_optional`
- **Incremental → Complete**: `max`, `to`, `for_each`, `count`, `accumulate` (accumulate/reduce then produce result)
- **Complete → Complete**: `sort`, `stable_sort`, `transform_complete`, `expand_complete`, `unwrap_optional_complete`
- **Complete → Incremental**: Automatic via chain component (iterates input, calls `process_incremental`)

### Iteration (`iterate.hpp`)

The iteration system determines how collection elements are forwarded to the first stage of a pipeline.

**Element Reference Categories**:

The reference type received by the first stage depends on how the collection is passed to `apply()`:

- **Lvalue container**: `vec` → Elements passed as `const T&`
  ```cpp
  std::vector<int> vec = {1, 2, 3};
  apply(vec, ...);  // First stage receives const int&
  ```

- **Mutable reference wrapper**: `std::ref(vec)` → Elements passed as `T&`
  ```cpp
  apply(std::ref(vec), ...);  // First stage receives int&
  ```

- **Rvalue container**: `std::move(vec)` → Elements passed as `T&&`
  ```cpp
  apply(std::move(vec), ...);  // First stage receives int&&
  ```

- **Generator output**: Produces whatever the generator's `output_type` specifies
  ```cpp
  // Generator<int, F> with output_type = int produces int
  apply(iota(1), ...);
  ```

**Implementation Details**:
- `unwrap_input(input)` - Normalizes input ranges:
  - `std::reference_wrapper<T>` → unwraps to `T&`
  - Generators and rvalue references → forwarded as-is
  - Lvalue references → adds `const` (becomes `const T&`)
- `iterate_output_t<Range>` - Determines element type preserving reference category:
  - For generators: uses `generator::output_type`
  - For containers: uses `forward_like<Range>(value_type)` to preserve reference category
  - Special case for rvalue `unordered_map`: extracts nodes, producing `pair<Key&&, Value&&>&&`
  - Special case for rvalue `unordered_set`: extracts nodes, producing `Value&&`
- `iterate(range, done, callback)` - Universal iteration:
  - Calls `unwrap_input()` first
  - Dispatches to `iterate_generator()` for generators
  - Dispatches to `iterate_like()` for containers (with special handling for unordered containers)

### Compositions (`compose.hpp`)
- `composition<Ts...>` - Tagged tuple for stage/range composition
- `forward_compose()` - Concatenates stages while preserving references
- `compose()` - User-facing API for merging pipelines

### Generators (`generator.hpp`)
- `generator<T, F>` - Lazy sequence with callback-based iteration
- `iota(start)` / `iota(start, end)` - Range generators
- Returns `true` to continue, `false` to stop

### Arguments (`args.hpp`)
Multi-argument support for stages through `args<Ts...>` wrapper:

- `args<Ts...>` - Custom tuple type for passing multiple arguments through pipeline
- `args_invoke(f, args)` - Expands args to function call with proper reference handling
- `get<I>(args)` - Access elements with reference semantics:
  - For `args<...>&&`: values become `T&&`, references preserved
  - For `const args<...>&`: everything becomes `const T&`
  - Non-const lvalue `args<...>&` is deleted (unclear semantics)
- Used by: `expand()`, `zip_result()`, `flatten()`, `swizzle()`, `enumerate()`, `transform_arg()`

**Reference Semantics**:
```cpp
args<int, bool&, const char&, float&&> a;

// Moved args (args&&)
args_invoke(f, std::move(a));
// → f(int&&, bool&, const char&, float&&)

// Const reference args (const args&)
args_invoke(f, std::as_const(a));
// → f(const int&, const bool&, const char&, const float&)
```

## Pipeline Execution

1. `apply(range, stages...)` entry point
2. `forward_compose()` flattens compositions into single tuple
3. `make_chain()` builds chain via variadic inheritance
4. Each stage's `make_impl<InputType>()` creates concrete implementation
5. First stage's `process_complete(range)` starts execution
6. Chain component dispatches based on stage style:
   - If stage has complete input (`has_complete_input<Stage>()`): calls `stage.process_complete(in, next)`
   - If stage has incremental input (`has_incremental_input<Stage>()`): calls `unwrap_input()`, then iterates and calls `stage.process_incremental(element, next)` for each element
7. Elements flow through pipeline via `process_incremental()` calls
8. Reducing stages accumulate in `process_incremental()`, then call `end(next)` to forward final result

## Reference Semantics

- `unwrap_input()` determines how the input range is processed (see iterate.hpp):
  - Lvalue ranges → adds `const`, producing `const T&` elements
  - Rvalue ranges → forwarded as-is, producing `T&&` elements
  - `std::ref(x)` → unwrapped to mutable `T&` elements
- Preserves move semantics through entire pipeline

### Result Finalization (`finalize.hpp`)

All pipeline results pass through `finalize()` before being returned to the user. This provides value semantics by default, following the same conventions as `std::make_pair()` and `std::make_tuple()`.

**Purpose**: Provide safe, predictable value semantics by default while allowing opt-in for references via `std::reference_wrapper<T>`.

**Rationale**:
- References in pipeline results are potentially dangerous (may dangle after pipeline ends)
- Value semantics by default is safer and matches standard library conventions
- Users who need references can explicitly use `std::reference_wrapper<T>`

**Behavior**:

1. **Tuple-like types** (`args<>`, `tuple<>`, `pair<>`):
   - ALL references are converted to values: `T&`, `const T&`, `T&&` → `T`
   - `std::reference_wrapper<T>` is unwrapped to `T&` (and contents are NOT converted)
   - Recursively applied to nested tuple-like types

2. **Non-tuple types**:
   - Converted to value type (return type is `auto`, not `decltype(auto)`)
   - `std::reference_wrapper` at top level is preserved as-is

**Examples** (from `tests/test_finalize.cpp`):
```cpp
// All references converted to values
args<int, bool&, const double&, float&&>           → args<int, bool, double, float>
tuple<int&, const double&&>                        → tuple<int, double>

// reference_wrapper unwrapped to references
args<std::reference_wrapper<int>>                  → args<int&>
args<std::reference_wrapper<const long>>           → args<const long&>

// reference_wrapper preserves inner references
args<std::reference_wrapper<pair<int&, double&&>>> → args<pair<int&, double&&>&>

// Nested tuple-like types
tuple<pair<int&, const double&>, args<float&&>>    → tuple<pair<int, double>, args<float>>

// Non-tuple types
std::vector<int>&&                                 → std::vector<int>
const std::string&                                 → std::string

// Top-level reference_wrapper preserved
std::reference_wrapper<int>                        → std::reference_wrapper<int>
```

**Opting into references**: To return references from a pipeline, wrap them with `std::reference_wrapper` (e.g., using a custom stage or `std::ref()`):
```cpp
auto result = dd::apply(
    vec,
    dd::transform([](auto& x) { return std::ref(x); }),  // Wrap with ref
    dd::to<std::vector>()
);
// result: vector<std::reference_wrapper<T>> → can access original elements
```

## Advanced Stages

### `flatten()` and `flatten_forward()`

Nested iteration over last argument of tuple:
```cpp
// Input: args<const int&, vector<int>&>
// Iterates vector, produces: args<const int&, int&>
```

**Reference Conversion for Safety:**

The `flatten()` stage passes through N-1 "prefix" arguments while iterating over the last argument (which may produce M iterations). This creates a critical semantic issue: rvalue references (`T&&`) semantically mean "this object is about to be destroyed, you can move from it," but flatten calls the next stage multiple times with the same arguments.

To prevent use-after-move bugs, `flatten()` applies these conversions to **prefix arguments only**:
- `T&` → `T&` (preserved - safe to mutate repeatedly)
- `const T&` → `const T&` (preserved - always safe)
- `T&&` → `const T&` (**converted** - prevents moving from object that lives through N iterations)

```cpp
// Example of the problem flatten() prevents:
dd::apply(
    get_users(),
    dd::zip_result([](User&& user) {
        return user.get_addresses();  // Returns vector<Address> with 3 elements
    }),
    dd::flatten(),  // Calls next stage 3 times with same User&&!
    dd::for_each([](User&& user, Address addr) {
        // ⚠️ Without conversion: first iteration could move user.email,
        // leaving empty string for iterations 2 and 3
        send_email(std::move(user.email), addr);  // ← Won't compile with flatten()
    })
);

// With flatten(), the signature forces safety:
dd::for_each([](const User& user, Address addr) {  // T&& → const T&
    send_email(user.email, addr);  // ✓ Can't accidentally move
})
```

**Advanced: `flatten_forward()`**

For users who understand the risks and need to move from prefix arguments, `flatten_forward()` preserves exact reference types:

```cpp
dd::apply(
    get_users(),
    dd::zip_result([](User&& user) {
        return user.get_addresses();
    }),
    dd::take_n(1),  // ← Ensure only ONE iteration
    dd::flatten_forward(),  // Preserves User&& (dangerous!)
    dd::for_each([](User&& user, Address addr) {
        send_email(std::move(user.email), addr);  // ✓ Safe because take_n(1)
    })
);
```

**Design Rationale:**

This is safe by default with explicit opt-in for advanced cases, similar to how `std::move` explicitly signals intent. The conversion only affects pass-through arguments; the iterated element's reference category is determined by normal iteration rules.

### `zip_result(f)`
Appends function result to arguments:
```cpp
// Input: int
// Output: args<const int&, result_type<f>&&>
```

### `swizzle<I...>()`
Reorders arguments.

### `enumerate<Index>(start)`
Prepends an incrementing index to each element:
```cpp
// Input: int
// Output: args<Index, int>

// Example:
dd::apply(
    vec,
    dd::enumerate(),  // defaults to Index=int, start=0
    dd::for_each([](int idx, int val) {
        std::cout << idx << ": " << val << '\n';
    })
);
```

Properly handles inputs that are already `args<...>` by prepending the index as the first argument.

### `transform_arg<Index>(f)`
Transforms a specific argument in an `args<...>` tuple while preserving others.

## Higher-Order Stages (`higher_order.hpp`)

Higher-order stages create subchains for advanced processing patterns.

### `tee(subchains...)`
Processes each input element through multiple independent subchains, collecting results as a tuple:

```cpp
dd::apply(
    vec,
    dd::tee(
        dd::compose(dd::filter(is_even), dd::count()),  // Count evens
        dd::compose(dd::transform(square), dd::max())   // Max of squares
    ),
    dd::expand_complete(),  // Expand tuple to args
    dd::for_each([](size_t count, optional<int> max) { ... })
);
```

**Properties**:
- Incremental → Complete
- Creates N independent chains from provided compositions
- Each input element is processed (as `const&`) by all subchains
- Output: `tuple<Result1, Result2, ...>`

### `map_group_by<Map>(key_getter, stages...)`
Groups elements by key and processes each group through a chain:

```cpp
dd::apply(
    vec,
    dd::map_group_by<std::map>(
        [](int x) { return x % 2; },  // Group by even/odd
        dd::transform([](int x) { return x * 10; }),
        dd::to<std::vector>()
    ),
    dd::for_each([](int key, std::vector<int> group) {
        std::cout << "Key " << key << ": " << group.size() << " items\n";
    })
);
```

**Properties**:
- Incremental → Incremental
- Template parameter: Map container template (e.g., `std::map`, `std::unordered_map`)
- Creates one chain instance per unique key
- Output: `args<Key, ChainResult>` for each group

## Debug System (`debug.hpp`)

### `apply_debug()`
Executes a pipeline with debug output showing the chain structure:

```cpp
// Prints to std::cout by default
auto result = dd::apply_debug(
    range,
    dd::transform(...),
    dd::filter(...),
    dd::to<std::vector>()
);

// Or specify output stream
dd::apply_debug(std::cerr, range, stages...);
```

**Debug Output Includes**:
- Stage index, type, and processing style (incremental/complete)
- Input and output types for each stage
- Custom information for higher-order stages:
  - `tee`: Shows structure of all subchains
  - `map_group_by`: Shows key type and per-group chain structure

**Customization**:
To add custom debug output for a stage, specialize `custom_debug_printer<StageType>`:

```cpp
namespace descend::detail {
template <>
struct custom_debug_printer<my_custom_stage>
{
    template <class StageImpl>
    static void print(std::ostream& strm, std::size_t depth)
    {
        strm << indent(depth) << "  Custom info here\n";
    }
};
}
```

## Complete → Complete Stages

These stages operate on entire collections and produce complete outputs. They are useful for operations that must see all data at once:

### `sort(comp = std::less<>)` / `stable_sort(comp = std::less<>)`
Sort collection in-place using provided comparator. Requires:
- Range with random access iterators
- Assignable elements
- Use `std::ref()` to sort lvalue ranges in-place

```cpp
std::vector<int> data = {3, 1, 4, 1, 5};
auto size = dd::apply(
    std::ref(data),  // std::ref for in-place modification
    dd::sort(),
    dd::transform_complete([](auto& vec) { return vec.size(); })
);
// data is now sorted: {1, 1, 3, 4, 5}
```

### `transform_complete(f)`
Transforms entire collection at once. Function receives unwrapped input:

```cpp
auto size = dd::apply(
    std::vector{1, 2, 3},
    dd::transform_complete([](auto&& vec) {
        return vec.size(); // vec is vector<int>&&
    })
);
```

## Type Flow Example

```cpp
dd::apply(
    vector<int>{1,2,3},              // output: vector<int>&&
    dd::transform([](int x) { return x * 2; }),  // input: int&&, output: int
    dd::filter([](int x) { return x > 3; }),     // input: int&&, output: int&&
    dd::to<std::vector>()            // output: vector<int>
);
```

Chain: `vector<int>` -> `transform_stage::impl<int&&>` -> `filter_stage::impl<int&&>` -> `to_stage::impl<int&&>`

## Processing Mode Details

The chain component provides two overloads of `process_complete`, selected via `requires` clauses:

**For stages with complete input:**
```cpp
constexpr decltype(auto) process_complete(input_type&& input)
    requires (has_complete_input<stage_type>())
{
    return m_stage_impl.process_complete((input_type&&) input, next());
}
```

**For stages with incremental input (automatic Complete → Incremental conversion):**
```cpp
template <class Input>
constexpr decltype(auto) process_complete(Input&& input)
    requires (has_incremental_input<stage_type>())
{
    detail::iterate(
            (Input&&) input,
            [this] () { return done(); },
            [this] (input_type&& elem) {
                m_stage_impl.process_incremental((input_type&&) elem, next());
            });
    return end();
}
```

This allows:
- Containers/generators at pipeline start automatically iterate to first stage
- Stages can choose incremental or complete processing based on their style
- Complete → Incremental conversion is transparent and automatic

## Implementation Notes

- Heavy use of SFINAE, concepts, and `if constexpr`
- Type deduction relies on `decltype(auto)` and perfect forwarding
- Debug printing via `debug_print_chain()` shows stage types
- Processing mode selection via SFINAE on `requires` clause

## Complete Stage Reference

### Incremental → Incremental
| Stage | Description | Example |
|-------|-------------|---------|
| `transform(f)` | Map function over elements | `transform([](int x) { return x * 2; })` |
| `filter(pred)` | Keep elements matching predicate | `filter([](int x) { return x > 0; })` |
| `take_n(n)` | Take first n elements | `take_n(10)` |
| `enumerate<Index>(start)` | Prepend incrementing index | `enumerate()` or `enumerate<size_t>(1)` |
| `expand()` | Expand tuples to args | Converts `tuple<int, bool>` → `args<int, bool>` |
| `zip_result(f)` | Append function result | Input `x` → `args<x, f(x)>` |
| `swizzle<I...>()` | Reorder args elements | `swizzle<1, 0>()` swaps first two args |
| `flatten()` | Flatten nested iteration (safe) | Last arg iterated, T&& → const T& for others |
| `flatten_forward()` | Flatten nested iteration (unsafe) | Preserves T&& (use with care) |
| `transform_arg<I>(f)` | Transform specific arg | `transform_arg<0>([](int x) { return x * 2; })` |
| `unwrap_optional()` | Unwrap optional or short-circuit | `optional<T>` → `T`, nullopt stops chain |

### Incremental → Complete
| Stage | Description | Output Type |
|-------|-------------|-------------|
| `max()` | Find maximum | `optional<T>` |
| `count()` | Count elements | `size_t` |
| `accumulate(init, op)` | Reduce with binary op | Type of `init` (or first element) |
| `to<Container>()` | Collect to container | `Container<T>` |
| `for_each(f)` | Apply side effect (terminal) | `void` |

### Complete → Complete
| Stage | Description | Notes |
|-------|-------------|-------|
| `sort(comp)` | Sort in-place | Requires std::ref for lvalues |
| `stable_sort(comp)` | Stable sort in-place | Requires std::ref for lvalues |
| `transform_complete(f)` | Transform entire collection | Receives unwrapped input |
| `expand_complete()` | Expand at collection level | Like `expand()` but for complete |
| `unwrap_optional_complete()` | Unwrap optional at collection level | Like `unwrap_optional()` but for complete |

### Higher-Order
| Stage | Description | Example |
|-------|-------------|---------|
| `tee(chains...)` | Process through multiple pipelines | `tee(compose(filter(...), count()), compose(max()))` |
| `map_group_by<Map>(key, stages...)` | Group by key, process each group | `map_group_by<std::map>([](auto x) { return x.category; }, count())` |

