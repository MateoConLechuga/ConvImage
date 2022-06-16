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

#include "icon.h"
#include "image.h"
#include "log.h"

#include "deps/libimagequant/libimagequant.h"

#include <string.h>
#include <errno.h>

static uint8_t icon_palette[];

int icon_convert(struct icon *icon)
{
    struct image image;
    liq_attr *liqattr = NULL;
    liq_image *liqimage = NULL;
    liq_result *liqresult = NULL;
    uint8_t *data = NULL;
    FILE *fd = NULL;
    bool has_icon = false;
    int ret = -1;

    if (icon->image_file != NULL)
    {
        has_icon = true;

        image_init(&image, icon->image_file);

        ret = image_load(&image);
        if (ret != 0)
        {
            LOG_ERROR("Failed loading icon image file.\n");
            goto fail;
        }

        if (image.width > 255)
        {
            LOG_ERROR("Icon \'%s\' width is %d. "
                      "Maximum supported is 255.\n",
                image.path,
                image.width);
            goto fail;
        }

        if (image.height > 255)
        {
            LOG_ERROR("Icon \'%s\' height is %d. "
                      "Maximum supported is 255.\n",
                image.path,
                image.height);
            goto fail;
        }

        if (image.width != 16 || image.height != 16)
        {
            LOG_WARNING("Icon \'%s\' is not 16x16 pixels.\n", image.path);
        }

        liqattr = liq_attr_create();
        if (liqattr == NULL)
        {
            LOG_ERROR("Failed creating icon image attributes.\n");
            goto fail;
        }

        liqimage = liq_image_create_rgba(liqattr, image.data, image.width, image.height, 0);
        if (liqimage == NULL)
        {
            LOG_ERROR("Failed creating icon image data.\n");
            goto fail;
        }

        for (unsigned int i = 0; i < 256; ++i)
        {
            unsigned int o = i * 3;
            liq_color liqcolor;

            liqcolor.r = icon_palette[o + 0];
            liqcolor.g = icon_palette[o + 1];
            liqcolor.b = icon_palette[o + 2];
            liqcolor.a = 255;

            liq_image_add_fixed_color(liqimage, liqcolor);
        }

        data = malloc(image.size);
        if (data == NULL)
        {
            LOG_ERROR("Memory error in \'%s\'.\n", __func__);
            goto fail;
        }

        liqresult = liq_quantize_image(liqattr, liqimage);
        if (liqresult == NULL)
        {
            LOG_ERROR("Could not quantize icon image.\n");
            goto fail;
        }

        liq_write_remapped_image(liqresult, liqimage, data, image.size);
    }

    fd = fopen(icon->output_file, "wt");
    if (fd == NULL)
    {
        LOG_ERROR("Could not open output file: %s\n", strerror(errno));
        goto fail;
    }

    switch (icon->format)
    {
        default:
            // fall through
        case ICON_FORMAT_ASM:
            fprintf(fd, "\tsection .icon\n\n");
            fprintf(fd, "\tjp\t___prgm_init\n");
            if (has_icon)
            {
                fprintf(fd, "\tdb\t$01\n");
                fprintf(fd, "\tpublic ___icon\n");
                fprintf(fd, "___icon:\n");
                fprintf(fd, "\tdb\t$%02X, $%02X", image.width, image.height);
                for (int y = 0; y < image.height; y++)
                {
                    int offset = y * image.width;

                    fprintf(fd, "\n\tdb\t");
                    for (int x = 0; x < image.width; x++)
                    {
                        if (x + 1 == image.width)
                        {
                            fprintf(fd, "$%02X", data[x + offset]);
                        }
                        else
                        {
                            fprintf(fd, "$%02X, ", data[x + offset]);
                        }
                    }
                }
            }
            else
            {
                fprintf(fd, "\tdb\t$02\n");
            }

            fprintf(fd, "\n");
            if (icon->description != NULL && icon->description[0])
            {
                fprintf(fd, "\tpublic ___description\n");
                fprintf(fd, "___description:\n");
                fprintf(fd, "\tdb\t\"%s\", 0\n", icon->description);
            }
            else
            {
                fprintf(fd, "\tpublic ___description\n");
                fprintf(fd, "___description:\n");
                fprintf(fd, "\tdb\t0\n");
            }
            fprintf(fd, "___prgm_init:\n");
            break;

        case ICON_FORMAT_ICE:
            if (has_icon)
            {
                fprintf(fd, "\"01%02X%02X", image.width, image.height);
                for (int y = 0; y < image.height; y++)
                {
                    int offset = y * image.width;

                    for (int x = 0; x < image.width; x++)
                    {
                        fprintf(fd, "%02X", data[x + offset]);
                    }
                }
                fprintf(fd, "\"\n");
            }
            break;
    }

    fclose(fd);

    ret = 0;

fail:

    if (has_icon)
    {
        liq_result_destroy(liqresult);
        liq_image_destroy(liqimage);
        liq_attr_destroy(liqattr);
        image_free(&image);
        free(data);
    }

    return ret;
}

static uint8_t icon_palette[] =
{
    0x00,0x00,0x00,  0x00,0x20,0x08,  0x00,0x41,0x10,  0x00,0x61,0x18,
    0x00,0x82,0x21,  0x00,0xA2,0x29,  0x00,0xC3,0x31,  0x00,0xE3,0x39,
    0x08,0x00,0x42,  0x08,0x20,0x4A,  0x08,0x41,0x52,  0x08,0x61,0x5A,
    0x08,0x82,0x63,  0x08,0xA2,0x6B,  0x08,0xC3,0x73,  0x08,0xE3,0x7B,
    0x10,0x00,0x84,  0x10,0x20,0x8C,  0x10,0x41,0x94,  0x10,0x61,0x9C,
    0x10,0x82,0xA5,  0x10,0xA2,0xAD,  0x10,0xC3,0xB5,  0x10,0xE3,0xBD,
    0x18,0x00,0xC6,  0x18,0x20,0xCE,  0x18,0x41,0xD6,  0x18,0x61,0xDE,
    0x18,0x82,0xE7,  0x18,0xA2,0xEF,  0x18,0xC3,0xF7,  0x18,0xE3,0xFF,
    0x21,0x04,0x00,  0x21,0x24,0x08,  0x21,0x45,0x10,  0x21,0x65,0x18,
    0x21,0x86,0x21,  0x21,0xA6,0x29,  0x21,0xC7,0x31,  0x21,0xE7,0x39,
    0x29,0x04,0x42,  0x29,0x24,0x4A,  0x29,0x45,0x52,  0x29,0x65,0x5A,
    0x29,0x86,0x63,  0x29,0xA6,0x6B,  0x29,0xC7,0x73,  0x29,0xE7,0x7B,
    0x31,0x04,0x84,  0x31,0x24,0x8C,  0x31,0x45,0x94,  0x31,0x65,0x9C,
    0x31,0x86,0xA5,  0x31,0xA6,0xAD,  0x31,0xC7,0xB5,  0x31,0xE7,0xBD,
    0x39,0x04,0xC6,  0x39,0x24,0xCE,  0x39,0x45,0xD6,  0x39,0x65,0xDE,
    0x39,0x86,0xE7,  0x39,0xA6,0xEF,  0x39,0xC7,0xF7,  0x39,0xE7,0xFF,
    0x42,0x08,0x00,  0x42,0x28,0x08,  0x42,0x49,0x10,  0x42,0x69,0x18,
    0x42,0x8A,0x21,  0x42,0xAA,0x29,  0x42,0xCB,0x31,  0x42,0xEB,0x39,
    0x4A,0x08,0x42,  0x4A,0x28,0x4A,  0x4A,0x49,0x52,  0x4A,0x69,0x5A,
    0x4A,0x8A,0x63,  0x4A,0xAA,0x6B,  0x4A,0xCB,0x73,  0x4A,0xEB,0x7B,
    0x52,0x08,0x84,  0x52,0x28,0x8C,  0x52,0x49,0x94,  0x52,0x69,0x9C,
    0x52,0x8A,0xA5,  0x52,0xAA,0xAD,  0x52,0xCB,0xB5,  0x52,0xEB,0xBD,
    0x5A,0x08,0xC6,  0x5A,0x28,0xCE,  0x5A,0x49,0xD6,  0x5A,0x69,0xDE,
    0x5A,0x8A,0xE7,  0x5A,0xAA,0xEF,  0x5A,0xCB,0xF7,  0x5A,0xEB,0xFF,
    0x63,0x0C,0x00,  0x63,0x2C,0x08,  0x63,0x4D,0x10,  0x63,0x6D,0x18,
    0x63,0x8E,0x21,  0x63,0xAE,0x29,  0x63,0xCF,0x31,  0x63,0xEF,0x39,
    0x6B,0x0C,0x42,  0x6B,0x2C,0x4A,  0x6B,0x4D,0x52,  0x6B,0x6D,0x5A,
    0x6B,0x8E,0x63,  0x6B,0xAE,0x6B,  0x6B,0xCF,0x73,  0x6B,0xEF,0x7B,
    0x73,0x0C,0x84,  0x73,0x2C,0x8C,  0x73,0x4D,0x94,  0x73,0x6D,0x9C,
    0x73,0x8E,0xA5,  0x73,0xAE,0xAD,  0x73,0xCF,0xB5,  0x73,0xEF,0xBD,
    0x7B,0x0C,0xC6,  0x7B,0x2C,0xCE,  0x7B,0x4D,0xD6,  0x7B,0x6D,0xDE,
    0x7B,0x8E,0xE7,  0x7B,0xAE,0xEF,  0x7B,0xCF,0xF7,  0x7B,0xEF,0xFF,
    0x84,0x10,0x00,  0x84,0x30,0x08,  0x84,0x51,0x10,  0x84,0x71,0x18,
    0x84,0x92,0x21,  0x84,0xB2,0x29,  0x84,0xD3,0x31,  0x84,0xF3,0x39,
    0x8C,0x10,0x42,  0x8C,0x30,0x4A,  0x8C,0x51,0x52,  0x8C,0x71,0x5A,
    0x8C,0x92,0x63,  0x8C,0xB2,0x6B,  0x8C,0xD3,0x73,  0x8C,0xF3,0x7B,
    0x94,0x10,0x84,  0x94,0x30,0x8C,  0x94,0x51,0x94,  0x94,0x71,0x9C,
    0x94,0x92,0xA5,  0x94,0xB2,0xAD,  0x94,0xD3,0xB5,  0x94,0xF3,0xBD,
    0x9C,0x10,0xC6,  0x9C,0x30,0xCE,  0x9C,0x51,0xD6,  0x9C,0x71,0xDE,
    0x9C,0x92,0xE7,  0x9C,0xB2,0xEF,  0x9C,0xD3,0xF7,  0x9C,0xF3,0xFF,
    0xA5,0x14,0x00,  0xA5,0x34,0x08,  0xA5,0x55,0x10,  0xA5,0x75,0x18,
    0xA5,0x96,0x21,  0xA5,0xB6,0x29,  0xA5,0xD7,0x31,  0xA5,0xF7,0x39,
    0xAD,0x14,0x42,  0xAD,0x34,0x4A,  0xAD,0x55,0x52,  0xAD,0x75,0x5A,
    0xAD,0x96,0x63,  0xAD,0xB6,0x6B,  0xAD,0xD7,0x73,  0xAD,0xF7,0x7B,
    0xB5,0x14,0x84,  0xB5,0x34,0x8C,  0xB5,0x55,0x94,  0xB5,0x75,0x9C,
    0xB5,0x96,0xA5,  0xB5,0xB6,0xAD,  0xB5,0xD7,0xB5,  0xB5,0xF7,0xBD,
    0xBD,0x14,0xC6,  0xBD,0x34,0xCE,  0xBD,0x55,0xD6,  0xBD,0x75,0xDE,
    0xBD,0x96,0xE7,  0xBD,0xB6,0xEF,  0xBD,0xD7,0xF7,  0xBD,0xF7,0xFF,
    0xC6,0x18,0x00,  0xC6,0x38,0x08,  0xC6,0x59,0x10,  0xC6,0x79,0x18,
    0xC6,0x9A,0x21,  0xC6,0xBA,0x29,  0xC6,0xDB,0x31,  0xC6,0xFB,0x39,
    0xCE,0x18,0x42,  0xCE,0x38,0x4A,  0xCE,0x59,0x52,  0xCE,0x79,0x5A,
    0xCE,0x9A,0x63,  0xCE,0xBA,0x6B,  0xCE,0xDB,0x73,  0xCE,0xFB,0x7B,
    0xD6,0x18,0x84,  0xD6,0x38,0x8C,  0xD6,0x59,0x94,  0xD6,0x79,0x9C,
    0xD6,0x9A,0xA5,  0xD6,0xBA,0xAD,  0xD6,0xDB,0xB5,  0xD6,0xFB,0xBD,
    0xDE,0x18,0xC6,  0xDE,0x38,0xCE,  0xDE,0x59,0xD6,  0xDE,0x79,0xDE,
    0xDE,0x9A,0xE7,  0xDE,0xBA,0xEF,  0xDE,0xDB,0xF7,  0xDE,0xFB,0xFF,
    0xE7,0x1C,0x00,  0xE7,0x3C,0x08,  0xE7,0x5D,0x10,  0xE7,0x7D,0x18,
    0xE7,0x9E,0x21,  0xE7,0xBE,0x29,  0xE7,0xDF,0x31,  0xE7,0xFF,0x39,
    0xEF,0x1C,0x42,  0xEF,0x3C,0x4A,  0xEF,0x5D,0x52,  0xEF,0x7D,0x5A,
    0xEF,0x9E,0x63,  0xEF,0xBE,0x6B,  0xEF,0xDF,0x73,  0xEF,0xFF,0x7B,
    0xF7,0x1C,0x84,  0xF7,0x3C,0x8C,  0xF7,0x5D,0x94,  0xF7,0x7D,0x9C,
    0xF7,0x9E,0xA5,  0xF7,0xBE,0xAD,  0xF7,0xDF,0xB5,  0xF7,0xFF,0xBD,
    0xFF,0x1C,0xC6,  0xFF,0x3C,0xCE,  0xFF,0x5D,0xD6,  0xFF,0x7D,0xDE,
    0xFF,0x9E,0xE7,  0xFF,0xBE,0xEF,  0xFF,0xDF,0xF7,  0xFF,0xFF,0xFF,
};
