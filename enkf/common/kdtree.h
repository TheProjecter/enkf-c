/******************************************************************************
 *
 * File:        kdtree.h        
 *
 * Created:     12/2012
 *
 * Author:      John Tsiombikas <nuclear@siggraph.org>
 *
 * Description:
 *
 * Revisions:   April 2012 - modified by Pavel Sakov
 *
 *****************************************************************************/

/*
This file is part of ``kdtree'', a library for working with kd-trees.
Copyright (C) 2007-2009 John Tsiombikas <nuclear@siggraph.org>

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
3. The name of the author may not be used to endorse or promote products
   derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
OF SUCH DAMAGE.
*/

#if !defined(_KDTREE_H_)

struct kdtree;
typedef struct kdtree kdtree;

struct kdnode;
typedef struct kdnode kdnode;

struct kdset;
typedef struct kdset kdset;

/* create a kd-tree for "k"-dimensional data */
kdtree* kd_create(int k);

/* free the kdtree */
void kd_destroy(kdtree* tree);

/* remove all the elements from the tree */
void kd_clear(kdtree* tree);

/* insert a node, specifying its position, and optional data */
int kd_insert(kdtree* tree, const double* pos);

/* Find any nearest nodes from the specified point within a range.
 *
 * This function returns a pointer to a result set, which can be manipulated
 * by the kd_res_* functions.
 * The returned pointer can be null as an indication of an error. Otherwise
 * a valid result set is always returned which may contain 0 or more elements.
 * The result set must be deallocated with kd_res_free, after use.
 */
kdset* kd_nearest_range(kdtree* tree, const double* pos, double range, int ordered);

/* frees a result set returned by kd_nearest_range() */
void kd_res_free(kdset* set);

/* gets id for the current item from the result set */
int kd_res_item_getid(kdset* set);

/* advances the result set iterator, returns non-zero on success, zero if
 * there are no more elements in the result set.
 */
int kd_res_next(kdset* set);

double* kd_res_getpos(kdset* set);

int kd_getid(kdnode* node);

#define _KDTREE_H_
#endif                          /* _KDTREE_H_ */
