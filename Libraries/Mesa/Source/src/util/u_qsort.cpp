/*
 * Copyright © 2020 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "u_qsort.h"
#if !defined(__lineos__)
#include <thread>
#endif

#if defined(__lineos__)
#define MESA_QSORT_LOCAL static
#else
#define MESA_QSORT_LOCAL thread_local
#endif

MESA_QSORT_LOCAL int (*tl_qsort_r_compar)(const void *, const void *, void *);
MESA_QSORT_LOCAL void *tl_qsort_r_arg;

static int
qsort_r_compar(const void *a, const void *b)
{
   return tl_qsort_r_compar(a, b, tl_qsort_r_arg);
}

void
util_tls_qsort_r(void *base, size_t nmemb, size_t size,
                 int (*compar)(const void *, const void *, void *),
                 void *arg)
{
   auto SavedCompare = tl_qsort_r_compar;
   void *SavedArgument = tl_qsort_r_arg;
   tl_qsort_r_compar = compar;
   tl_qsort_r_arg = arg;
   qsort(base, nmemb, size, qsort_r_compar);
   tl_qsort_r_compar = SavedCompare;
   tl_qsort_r_arg = SavedArgument;
}

int
util_qsort_adapter(void *ctx, const void *elem1, const void *elem2)
{
   util_qsort_adapter_data *data =
      reinterpret_cast<util_qsort_adapter_data*>(ctx);
   return data->compar(elem1, elem2, data->args);
}
