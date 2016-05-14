/**
 *******************************************************************************
 * @file    rcu.h
 * @author  Olli Vanhoja
 * @brief   Realtime friendly Read-Copy-Update.
 * @section LICENSE
 * Copyright (c) 2016 Olli Vanhoja <olli.vanhoja@cs.helsinki.fi>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *******************************************************************************
 */

/**
 * @addtogroup rcu
 * @{
 */

#pragma once
#ifndef RCU_H
#define RCU_H

/**
 * Opaque RCU reader lock context returned by rcu_read_lock().
 */
struct rcu_lock_ctx {
    int selector;
};

/**
 * Opaque RCU control block.
 * Optimally this struct is a member of a parent struct that is
 * the actual resource being managed with RCU.
 */
struct rcu_cb {
    void (*callback)(struct rcu_cb *);
    struct rcu_cb * callback_arg;
    struct rcu_cb * next;
};

/**
 * Take a reader lock.
 */
struct rcu_lock_ctx rcu_read_lock(void);

/**
 * Release a reader lock.
 */
void rcu_read_unlock(struct rcu_lock_ctx * restrict ctx);

/**
 * Assign an RCU managed pointer.
 */
#define rcu_assign_pointer(p, v) ({ \
    cpu_wmb();                      \
    ACCESS_ONCE(p) = (v);           \
})

/**
 * Dereference an RCU managed pointer.
 */
#define rcu_dereference(p) ({           \
    typeof(p) _value = ACCESS_ONCE(p);  \
    (_value);                           \
})


/**
 * Register a callback for freeing up the resources.
 * @note Should be called only once per cbd.
 * @param cbd is a pointer to the RCU control block.
 */
void rcu_call(struct rcu_cb * cb, void (*fn)(struct rcu_cb *));

/**
 * Wait for all RCU readers to unlock.
 */
void rcu_synchronize(void);

/* TODO RCU list handling */

#endif /* RCU_H */

/**
 * @}
 */
