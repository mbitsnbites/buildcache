-- match(dummy)

-------------------------------------------------------------------------------
-- Copyright (c) 2021 Marcus Geelnard
--
-- This software is provided 'as-is', without any express or implied warranty.
-- In no event will the authors be held liable for any damages arising from the
-- use of this software.
--
-- Permission is granted to anyone to use this software for any purpose,
-- including commercial applications, and to alter it and redistribute it
-- freely, subject to the following restrictions:
--
--  1. The origin of this software must not be misrepresented; you must not
--     claim that you wrote the original software. If you use this software in
--     a product, an acknowledgment in the product documentation would be
--     appreciated but is not required.
--
--  2. Altered source versions must be plainly marked as such, and must not be
--     misrepresented as being the original software.
--
--  3. This notice may not be removed or altered from any source distribution.
-------------------------------------------------------------------------------

require_std("io")

function get_capabilities ()
  return {'direct_mode'}
end

function get_build_files ()
  local files = {}
  local found_output_file = false
  for i = 2, #ARGS do
    local next_idx = i + 1
    if (ARGS[i] == "-o") or (ARGS[i] == "--output") and (next_idx <= #ARGS) then
      if found_output_file then
        error("Only a single output file can be specified.")
      end
      files["output"] = ARGS[next_idx]
      found_output_file = true
    end
  end
  if not found_output_file then
    error("Unable to get the output file.")
  end
  return files
end

function get_relevant_arguments ()
  -- There are currently no relevant arguments that affect the output file.
  return {}
end

function get_input_files ()
  -- We assume that the last argument is the source file.
  return {ARGS[#ARGS]}
end

function preprocess_source ()
  -- We assume that the last argument is the source file.
  local source_path = ARGS[#ARGS]

  -- Use the content of the source file for the hash.
  local f = assert(io.open(source_path, "rb"))
  local preprocessed_source = f:read("*all")
  f:close()

  return preprocessed_source
end
