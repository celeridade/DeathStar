#pragma once

// The injected dll is embedded in the loader executable as a binary resource,
// so a single .exe carries everything. The loader extracts it to a temp file at
// inject time because LoadLibrary needs a path on disk.
#define IDR_OMEGADLL 101
