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
/// @brief Support for malloc()/calloc()/free().
/// Functions here are basically just pass-throughs to the underlying
/// heap management functions. This layer exists to keep code analysis
/// tools happy.
/// Say we want to add an environment variable. Putting it in a
/// static buffer runs the theoretical risk of buffer overflow.
/// Putting it on the heap is better all around. The problem is
/// that both can trigger code analysis complaints. It's perfectly
/// reasonable in practice to malloc() a little storage and add
/// it to the environment and know it will be cleaned up by exit()
/// automatically. Totally fine, but it will be reported by valgrind
/// and similar tools as a memory leak. And the static buffer may
/// trigger a possible-overrun warning, in addition to being dumb.
/// The solution used here takes advantage of a feature of the cJSON
/// library; a cJSON document is torn down and deallocated automatically
/// by one call to cJSON_Delete(root).
/// Therefore, memory allocated here may be attached to a JSON doc
/// which is explicitly destroyed at exit time. It's a bit of overkill
/// since exit() would do the same job for free but this bookkeeping
/// reduces the number of bogus memory-leak warnings, and maybe even
/// real memory leaks.

#include "Putil/putil.h"

#include "HEAP.h"

#include "cJSON.h"

cJSON *HeapJson = NULL;

/// Initialize the data structure that holds "permanent" allocations.
void
heap_init(void)
{
    TRY(HeapJson = cJSON_CreateArray(), "cJSON_CreateArray()");
}

/// Convert the supplied string to "permanent" status. The original will be
/// freed, so it must be on the heap, and the "permanent" copy is returned.
CS
heap_save(CS savestr)
{
    cJSON *saved;

    TRY(saved = cJSON_CreateString(savestr), "cJSON_CreateString()");
    TRY(cJSON_AddItemToArray(HeapJson, saved), "cJSON_AddItemToArray()");
    free(savestr);
    return cJSON_GetStringValue(saved);
}

/// Finalize and free "permanent" heap allocations.
void
heap_fini(void)
{
    cJSON_Delete(HeapJson);
    HeapJson = NULL;
}


// vim: sw=4 ts=4 et tw=100 cc=+1
