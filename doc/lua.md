# Using custom Lua plugins

It is possible to extend the capabilities of BuildCache with
[Lua](https://www.lua.org/). See [lua-examples](../lua-examples/) for some
examples of Lua wrappers.

## Location of wrapper scripts

BuildCache first searches for Lua scripts in the paths given in the environment
variable `BUILDCACHE_LUA_PATH` (colon separated on POSIX systems, and semicolon
separated on Windows), and then continues searching in `$BUILDCACHE_DIR/lua`.
If no matching script file was found, BuildCache falls back to the built in
compiler wrappers (as listed above).

## Wrapper identification

The first line of a Lua based program wrapper script must be a Lua comment with
a special "match"-statement that specifies a regex that matches the name of the
program that is to be wrapped, e.g:

```Lua
-- match(gcc.*)
```

More detailed checks can be done in the optional `can_handle_command` method.

## Anatomy of a wrapper

The following methods can be implemented (see
[program_wrapper.hpp](../src/wrappers/program_wrapper.hpp) for a more detailed
documentation):

| Function | Returns | Default |
| --- | --- | --- |
| can_handle_command () | Can the wrapper handle this program? | true |
| resolve_args () | (nothing) | - |
| get_capabilities () | A list of supported capabilities | An empty table |
| preprocess_source () | The preprocessed source code (e.g. for C/C++) | An empty string |
| get_relevant_arguments () | Arguments that can affect the build output | All arguments |
| get_relevant_env_vars () | Environment variables that can affect the build output | An empty table |
| get_relevant_external_files () | A list of external files that can affect program execution| An empty table |
| get_program_id () | A unique program identification | The MD4 hash of the program binary |
| get_build_files () | A table of build result files | An empty table |
| run_for_miss () | A `sys::run_result_t` compatible table | *See note\** |

\*: `run_for_miss`, when defined, shall run the actual command (as specified by
`ARGS`) if a cache miss occurs. The return value shall be a table consisting of
`std_out`, `std_err` and `return_code` (see
[sys::run_result_t](../src/sys/sys_utils.hpp)). The default implementation is
equivalent to `bcache.run(ARGS, false)`.

## Miscellaneous

To use Lua standard libraries (`coroutine`, `debug`, `io`, `math`, `os`,
`package`, `string`, `table` or `utf8`), you must first load them by calling
`require_std(name)`. For convenience it is possible to load all standard
libraries with `require_std("*")`, but beware that it is slower than to load
only the libraries that are actually used.

All program arguments are available in the global `ARGS` array (an array of
strings).

