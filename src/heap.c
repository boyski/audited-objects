// Copyright (c) 2002-2025 David Boyce.  All rights reserved.

/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/// @file
/// @brief Support for heap_{malloc,calloc,free,etc}().
/// Functions here are basically just pass-throughs to the underlying
/// heap management functions while recording the pointer in a JSON
/// structure. This layer exists to keep code analysis tools happy.
///
/// Say we want to add an environment variable. Putting it in a
/// static buffer runs the theoretical risk of buffer overflow.
/// Putting it on the heap is better all around, but the problem is
/// that both can trigger code analysis complaints. It's perfectly
/// reasonable in practice to malloc() a little storage and add
/// it to the environment and know it will be cleaned up by exit()
/// automatically. Totally fine, but it will be reported by valgrind
/// and static analysis tools as a memory leak. Whereas a static buffer
/// may trigger a possible-overrun warning in addition to being dumb.
///
/// The solution used here takes advantage of a feature of the cJSON
/// library; a cJSON document is torn down and deallocated automatically
/// by one call to cJSON_Delete(root).
///
/// Therefore, memory allocated here will be attached to a JSON doc
/// which is explicitly destroyed at exit time. It's a bit of overkill
/// since exit() would do the same job for free but this bookkeeping
/// reduces the number of bogus memory-leak warnings, and maybe even
/// some actual memory leaks.

#include "AO.h"
#include "FREE.h"
#include "HEAP.h"
#include "TRY.h"

#include "cJSON.h"

cJSON *HeapJson = NULL;

/// Initialize the data structure that holds "permanent" allocations.
void
heap_init(void)
{
    TRY(HeapJson = cJSON_CreateArray(), "cJSON_CreateArray()");
}

/// Convert the supplied string to "permanent" status. The original must be on
/// the heap. The input pointer remains valid and is returned. The only change
/// is that it will be freed automatically by heap_fini().
void *
heap_save(void *saveptr)
{
    if (getenv("AO_DEBUG")) {
        cJSON *saved = cJSON_CreateString(""); // Use a static string as a placeholder
        saved->valuestring = (char *)saveptr;  // Replace placeholder with saveptr
        TRY(cJSON_AddItemToArray(HeapJson, saved), "cJSON_AddItemToArray()");
    }
    return saveptr;
}

/// Allocate a new formatted string a la asprintf() and record the result
/// to be automatically freed by heap_fini().
CS
heap_sprintf(const char *fmt, ...)
{
    va_list args;

    CS newstr;
    va_start(args, fmt);
    TRY(vasprintf(&newstr, fmt, args) != -1, "vasprintf()");
    va_end(args);
    return heap_save(newstr);
}

void
heap_free(void *ptr)
{
    UNUSED(ptr); // TODO
}

/// Finalize and free "permanent" heap allocations.
void
heap_fini(void)
{
    if (HeapJson && getenv("AO_DEBUG")) {
        if (vb_bitmatch(VB_TMP)) {
            CS jsonstr = cJSON_Print(HeapJson);
            vb_printf(VB_TMP, "Freeing:\n%s\n", jsonstr); // causes core dump
            FREE(jsonstr);
        }
        cJSON_Delete(HeapJson);
        HeapJson = NULL;
    }
}


// vim: sw=4 ts=4 et tw=100 cc=+1
