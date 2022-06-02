/*
 * Copyright 2017-2022 Matt "MateoConLechuga" Waltz
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "compress.h"
#include "log.h"

#include "deps/zx/zx7/zx7.h"
#include "deps/zx/zx0/zx0.h"

#include <string.h>

static int compress_zx7(uint8_t *data, size_t *size)
{
    zx7_Optimal *opt;
    uint8_t *compressed_data;
    size_t new_size;
    long delta;

    if (size == NULL || data == NULL)
    {
        return -1;
    }

    opt = zx7_optimize(data, *size, 0);
    if (opt == NULL)
    {
        LOG_ERROR("Could not optimize zx7.\n");
        return -1;
    }

    compressed_data = zx7_compress(opt, data, *size, 0, &new_size, &delta);
    free(opt);
    if (compressed_data == NULL)
    {
        LOG_ERROR("Could not compress zx7.\n");
        return -1;
    }

    memcpy(data, compressed_data, new_size);
    *size = new_size;

    free(compressed_data);

    return 0;
}

static void compress_zx0_progress(void)
{
    LOG_PRINT(".");
}

static int compress_zx0(uint8_t *data, size_t *size)
{
    int delta;
    uint8_t *compressed_data;
    int new_size;

    if (size == NULL || data == NULL)
    {
        return -1;
    }

    LOG_PRINT("[info] Compressing [");

    compressed_data = zx0_compress(zx0_optimize(data, *size, 0, 2000, compress_zx0_progress),
                                   data, *size, 0, 0, 1, &new_size, &delta);
    LOG_PRINT("]\n");
    if (compressed_data == NULL)
    {
        LOG_ERROR("Could not compress zx0.\n");
        return -1;
    }

    memcpy(data, compressed_data, new_size);
    *size = new_size;

    free(compressed_data);

    return 0;
}

int compress_array(uint8_t *data, size_t *size, compress_t mode)
{
    switch (mode)
    {
        default:
        case COMPRESS_INVALID:
            return -1;

        case COMPRESS_NONE:
            return 0;

        case COMPRESS_ZX7:
            return compress_zx7(data, size);

        case COMPRESS_ZX0:
            return compress_zx0(data, size);
    }

    return 0;
}
