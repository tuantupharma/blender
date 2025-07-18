/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 *
 * SVG vector graphics format support for the purpose of thumbnail-display.
 * While loading these as an #ImBuf is trivial to support, it would expose
 * limitations of NANOSVG and users may end up needing more advanced options
 * specific to loading vector graphics (such as resolution control), see #109567 for details.
 */

#include "IMB_colormanagement.hh"
#include "IMB_filetype.hh"
#include "IMB_imbuf_types.hh"
#include "nanosvgrast.h"

ImBuf *imb_load_filepath_thumbnail_svg(const char *filepath,
                                       const int /*flags*/,
                                       const size_t max_thumb_size,
                                       ImFileColorSpace & /*r_colorspace*/,
                                       size_t *r_width,
                                       size_t *r_height)
{
  NSVGimage *image = nsvgParseFromFile(filepath, "px", 96.0f);

  if (image == nullptr) {
    return nullptr;
  }

  if (image->width == 0 || image->height == 0) {
    nsvgDelete(image);
    return nullptr;
  }

  int w = int(image->width);
  int h = int(image->height);

  /* Return full size of the image. */
  *r_width = size_t(w);
  *r_height = size_t(h);

  NSVGrasterizer *rast = nsvgCreateRasterizer();
  if (rast == nullptr) {
    nsvgDelete(image);
    return nullptr;
  }

  const float scale = float(max_thumb_size) / std::max(w, h);
  const int dest_w = std::max(int(w * scale), 1);
  const int dest_h = std::max(int(h * scale), 1);

  ImBuf *ibuf = IMB_allocImBuf(dest_w, dest_h, 32, IB_byte_data);
  if (ibuf != nullptr) {
    nsvgRasterize(rast, image, 0, 0, scale, ibuf->byte_buffer.data, dest_w, dest_h, dest_w * 4);
    IMB_flipy(ibuf);
  }

  nsvgDeleteRasterizer(rast);
  nsvgDelete(image);

  return ibuf;
}
