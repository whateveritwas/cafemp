/*
 * Automatically redirect stdout to WHBLogWrite().
 * Copyright 2026  Daniel K. O. (dkosmari)
 *
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-License-Identifier: LGPL-3.0-or-later
 * SPDX-License-Identifier: MIT
 *
 * Source: https://github.com/dkosmari/wiiu-stdout
 */

#ifndef WIIU_STDOUT_HPP
#define WIIU_STDOUT_HPP

#ifdef __WIIU__

#include <coreinit/mutex.h>

/*
 * Call this manually when the automatic call doesn't work.
 * It's safe to call it multiple times.
 */
void
wiiu_init_stdout()
    noexcept;

/*
 * Call this manually when the automatic call doesn't work.
 * It's safe to call it multiple times.
 * This is called automatically by wiiu_init_stdout().
 */
void
wiiu_init_whb_log()
    noexcept;

/*
 * Call this manually when the automatic call doesn't work.
 * It's safe to call it multiple times.
 * If doing manual calls, this will ensure the log output is finalized.
 */
void
wiiu_fini_whb_log()
    noexcept;

/*
 * Used by wiiu-stderr.cpp
 */
extern OSMutex* wiiu_whb_log_mutex;

#endif // __WIIU__

#endif
