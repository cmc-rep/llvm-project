//===-- Implementation header for thrd_join function ------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC_THREADS_THRD_JOIN_H
#define LLVM_LIBC_SRC_THREADS_THRD_JOIN_H

#include "src/__support/macros/config.h"
#include <threads.h>

namespace LIBC_NAMESPACE_DECL {

int thrd_join(thrd_t thread, int *retval);

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC_THREADS_THRD_JOIN_H
