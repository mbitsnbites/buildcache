//--------------------------------------------------------------------------------------------------
// Copyright (c) 2020 Marcus Geelnard
//
// This software is provided 'as-is', without any express or implied warranty. In no event will the
// authors be held liable for any damages arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose, including commercial
// applications, and to alter it and redistribute it freely, subject to the following restrictions:
//
//  1. The origin of this software must not be misrepresented; you must not claim that you wrote
//     the original software. If you use this software in a product, an acknowledgment in the
//     product documentation would be appreciated but is not required.
//
//  2. Altered source versions must be plainly marked as such, and must not be misrepresented as
//     being the original software.
//
//  3. This notice may not be removed or altered from any source distribution.
//--------------------------------------------------------------------------------------------------

#include <base/env_utils.hpp>
#include <sys/filetracker.hpp>

#include <windows.h>

// Note: The FileTracker API has been created from msdn documentation.
// It seems the documentation may be slightly incorrect with respect to return values in case of
// error conditions, and possibly other topics.

/* As of toolchain 19.27.29111, FileTracker DLL is detouring:
CreateThread
CreateFile{A|W}
CopyFile{A|W|ExA|ExW}
MoveFile{A|W|ExA|ExW}
SetFileInformationByHandle
ReplaceFileW - (if TRACKER_TRACK_REPLACEFILE env var exists)
CreateHardLink{A|W}
CreateProcess{A|W}
CreateDirectory{A|W}
RemoveDirectory{A|W}
GetFileAttributes{A|W|ExA|ExW}
DeleteFile{A|W}
TerminateProcess
ExitProcess
DisableThreadLibraryCalls
*/

/// @brief Start a tracking context.
/// @param intermediateDirectory The directory in which to store the tracking log.
/// @param taskName Identifies the tracking context. This name is used to create the log file name.
/// @returns An HRESULT with the SUCCEEDED bit set if the tracking context was created.
HRESULT WINAPI StartTrackingContext(LPCTSTR intermediateDirectory, LPCTSTR taskName);

/// @brief Starts a tracking context using a response file specifying a root marker.
/// @param intermediateDirectory The directory in which to store the tracking log.
/// @param taskName Identifies the tracking context. This name is used to create the log file name.
/// @param rootMarkerResponseFile The pathname of a response file containing a root marker. The root
/// name is used to group all tracking for a context together.
/// @returns An HRESULT with the SUCCEEDED bit set if the tracking context was created.
HRESULT WINAPI StartTrackingContextWithRoot(LPCTSTR intermediateDirectory,
                                            LPCTSTR taskName,
                                            LPCTSTR rootMarkerResponseFile);

/// @brief End the current tracking context.
/// @returns An HRESULT with the SUCCEEDED bit set if the tracking context was ended.
HRESULT WINAPI EndTrackingContext();

/// @brief Stops all tracking and frees any memory used by the tracking session.
/// @returns Returns an HRESULT with the SUCCEEDED bit set if tracking was stopped.
HRESULT WINAPI StopTrackingAndCleanup(void);

/// @brief Suspends tracking in the current context.
/// @returns An HRESULT with the SUCCEEDED bit set if tracking was suspended.
HRESULT WINAPI SuspendTracking(void);

/// @brief Resumes tracking in the current context.
/// @returns An HRESULT with the SUCCEEDED bit set if tracking was resumed. E_FAIL is returned if
/// tracking cannot be resumed because the context was not available.
HRESULT WINAPI ResumeTracking();

/// @brief Writes tracking logs for all threads and contexts.
/// @param intermediateDirectory The directory in which to store the tracking log.
/// @param tlogRootName The root name of the log file name.
/// @note The msdn doc for return value is incorrect.
HRESULT WINAPI WriteAllTLogs(LPCTSTR intermediateDirectory, LPCTSTR tlogRootName);

/// @brief Writes logs files for the current context.
/// @param intermediateDirectory The directory in which to store the tracking log.
/// @param tlogRootName The root name of the log file name.
/// @note The msdn doc for return value is incorrect.
HRESULT WINAPI WriteContextTLogs(LPCTSTR intermediateDirectory, LPCTSTR tlogRootName);

/// @brief Sets the global thread count, and assigns that count to the current thread.
/// @param threadCount The number of threads to use.
/// @returns An HRESULT with the SUCCEEDED bit set if the thread count was updated.
HRESULT WINAPI SetThreadCount(int threadCount);

namespace bcache {
namespace filetracker {

HMODULE handle;
decltype(&::SuspendTracking) SuspendTracking;
decltype(&::ResumeTracking) ResumeTracking;

bool init() {
  if (handle) {
    return true;
  }
  env_var_t tracker_enabled("TRACKER_ENABLED");
  if (!tracker_enabled.as_bool()) {
    return false;
  }
  for (auto module_name : {"FileTracker64", "FileTracker32", "FileTracker"}) {
    handle = GetModuleHandleA(module_name);
    if (handle) {
      break;
    }
  }
  if (!handle) {
    return false;
  }
#define FUNC_RESOLVE(x)                          \
  do {                                           \
    x = (decltype(x))GetProcAddress(handle, #x); \
    if (!x) {                                    \
      handle = nullptr;                          \
      return false;                              \
    }                                            \
  } while (0)
  FUNC_RESOLVE(SuspendTracking);
  FUNC_RESOLVE(ResumeTracking);
#undef FUNC_RESOLVE
  return true;
}

void suspend() {
  if (!init()) {
    return;
  }
  SuspendTracking();
}

void resume() {
  if (!init()) {
    return;
  }
  ResumeTracking();
}

struct FileTrackerScopedSuppressor {
  FileTrackerScopedSuppressor() {
    suspend();
  }
  ~FileTrackerScopedSuppressor() {
    resume();
  }
};

// Ensure automatic suspend and resume of FileTracker for buildcache lifetime.
// Unbalanced calls to suspend/resume is OK.
static FileTrackerScopedSuppressor filetracker_singleton;

}  // namespace filetracker
}  // namespace bcache
