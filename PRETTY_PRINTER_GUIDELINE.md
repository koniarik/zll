# Pretty Printer Integration Guideline

This document describes a practical way to add a debugger pretty printer to a project, with special focus on testing.

## Goal

A pretty printer should make debugger output easier to understand without changing program behavior. Treat it as a debugger integration with a stable contract, not as an informal convenience script.

## Recommended structure

1. Keep the pretty printer in a separate debugger script.
2. Document a simple activation step such as sourcing the script from the debugger.
3. Base the printer on a small, well-defined runtime representation rather than duplicating logic for every public wrapper type.
4. Prefer exposing children in a way that lets the debugger render nested values naturally.
5. Handle null, empty, and optimized storage layouts explicitly.

## Design principles

### 1. Depend on a small internal contract

The printer should rely on as few implementation details as possible. If several public types share one runtime representation, print that shared representation and adapt thin wrappers to it.

Good contract candidates:

- active type discriminator
- pointer or storage location
- null or empty marker
- debugger-visible concrete type names

Avoid spreading layout knowledge across many printer classes.

### 2. Accept debugger reality

The debugger sees instantiated concrete types, not necessarily the public aliases users write in code. Register printers for the types the debugger actually reports.

### 3. Keep output simple

The printer should answer two questions clearly:

1. what kind of object this is
2. what value it currently holds

Use one child for the active value when possible. Let the debugger recurse into nested structures instead of manually formatting everything in Python.

### 4. Plan for layout special cases

Many data structures have more than one storage mode, for example:

- empty vs non-empty
- single-type optimized layout vs multi-type layout
- owning vs non-owning wrappers

Model these branches directly in the printer instead of hoping one generic path will work for all cases.

## Testing strategy

The most important lesson is this: **test the printer through the debugger, end to end**.

Do not rely only on unit-testing the Python script. A printer can look correct in isolation and still fail when the debugger loads symbols, resolves types, or renders values.

### Recommended test harness

Use a three-step pipeline:

1. **Generate a debugger script** that sets breakpoints and prints selected variables.
2. **Run the debugger in batch mode** on a dedicated test binary.
3. **Evaluate the captured output** against expected results.

This structure gives clear failure modes:

- script generation failed
- debugger invocation failed
- printed output did not match expectations

### Why a dedicated test binary works well

A small binary written only for printer tests gives full control over:

- exact variable names
- object lifetimes
- null and non-null states
- recursive or nested values
- line numbers used for breakpoints

This is better than trying to piggyback on a large existing test executable.

### Keep expectations close to the test values

The expected printer output should live next to the code that constructs the inspected values. One practical pattern is:

- create the value
- define the expected debugger output
- generate a breakpoint at that exact source location

That keeps maintenance simple when test code changes.

### Match stable output, not incidental formatting

Debugger output often contains unstable prefixes, counters, or addresses. Compare only the meaningful stable part of the rendered value.

Good examples:

- suffix match
- normalized whitespace
- capped recursion depth

Avoid assertions that depend on transient debugger formatting.

### Limit recursion depth

Recursive structures are common in the kinds of data that benefit from pretty printers. Configure the debugger to cap print depth so the output stays readable and deterministic.

### Test behavior, not implementation

Your assertions should describe what a developer sees in the debugger, not how the printer computed it.

Test cases should cover:

- empty or null state
- each active alternative or variant branch
- single-element and multi-element layouts
- nested values
- recursive values
- wrapper types sharing the same core representation
- user-facing strings or structured objects

## Suggested test matrix

Use at least these scenarios:

| Case | Why |
|---|---|
| Empty or null value | Confirms safe rendering of missing state |
| One active concrete type | Confirms the basic happy path |
| Multiple active concrete types | Confirms discriminator handling |
| Optimized layout | Confirms special storage cases |
| Nested object | Confirms child rendering works |
| Recursive object | Confirms depth control and stability |
| Multiple wrapper forms | Confirms a shared printer contract |

## Maintenance guidance

### When the implementation changes

If the runtime layout changes, update the printer and its debugger tests together. The tests should be treated as the executable specification for debugger output.

### Keep the contract small

If the printer starts depending on many private fields, it becomes brittle. Either reduce that coupling or introduce a clearer internal representation that the printer can target consistently.

### Make failures obvious

When output mismatches, report:

- expected text
- actual text
- source location of the test case

This keeps printer failures cheap to diagnose.

## Checklist for adding a pretty printer

1. Define the debugger-facing runtime contract.
2. Implement the printer around the shared core representation.
3. Register it for the concrete types the debugger actually sees.
4. Handle null and layout edge cases explicitly.
5. Add a dedicated debugger test binary.
6. Generate debugger commands automatically from the test source.
7. Run the debugger in batch mode in CI or the regular test workflow.
8. Compare only stable, user-meaningful output.
9. Add cases for nested and recursive structures.
10. Keep documentation to a one-command activation path.

## Short version

The safest way to add a pretty printer is:

- keep the printer small
- anchor it to a narrow runtime contract
- test it through the real debugger
- compare stable output from a dedicated test program

That gives a printer that is useful to developers and resilient to normal code evolution.
