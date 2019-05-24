//==============================================================================
//
//  MediaRouter
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

// VP8 Bitstream Sytax

// Reference: https://tools.ietf.org/html/rfc6386#page-120

/*
 This annex presents the bitstream syntax in a tabular form.  All the
   information elements have been introduced and explained in the
   previous sections but are collected here for a quick reference.  Each
   syntax element is briefly described after the tabular representation
   along with a reference to the corresponding paragraph in the main
   document.  The meaning of each syntax element value is not repeated
   here.

   The top-level hierarchy of the bitstream is introduced in Section 4.

   Definition of syntax element coding types can be found in Section 8.
   The types used in the representation in this annex are:

   o  f(n), n-bit value from stream (n successive bits, not boolean
      encoded)

   o  L(n), n-bit number encoded as n booleans (with equal probability
      of being 0 or 1)

   o  B(p), bool with probability p of being 0

   o  T, tree-encoded value

19.1.  Uncompressed Data Chunk

   | Frame Tag                                         | Type  |
   | ------------------------------------------------- | ----- |
   | frame_tag                                         | f(24) |
   | if (key_frame) {                                  |       |
   |     start_code                                    | f(24) |
   |     horizontal_size_code                          | f(16) |
   |     vertical_size_code                            | f(16) |
   | }                                                 |       |

   The 3-byte frame tag can be parsed as follows:

   ---- Begin code block --------------------------------------

   unsigned char *c = pbi->source;
   unsigned int tmp;

   tmp = (c[2] << 16) | (c[1] << 8) | c[0];

   key_frame = tmp & 0x1;
   version = (tmp >> 1) & 0x7;
   show_frame = (tmp >> 4) & 0x1;
   first_part_size = (tmp >> 5) & 0x7FFFF;

   ---- End code block ----------------------------------------

   Where:

   o  key_frame indicates whether the current frame is a key frame
      or not.

   o  version determines the bitstream version.

   o  show_frame indicates whether the current frame is meant to be
      displayed or not.

   o  first_part_size determines the size of the first partition
      (control partition), excluding the uncompressed data chunk.

   The start_code is a constant 3-byte pattern having value 0x9d012a.
   The latter part of the uncompressed chunk (after the start_code) can
   be parsed as follows:

   ---- Begin code block --------------------------------------

   unsigned char *c = pbi->source + 6;
   unsigned int tmp;

   tmp = (c[1] << 8) | c[0];

   width = tmp & 0x3FFF;
   horizontal_scale = tmp >> 14;

   tmp = (c[3] << 8) | c[2];

   height = tmp & 0x3FFF;
   vertical_scale = tmp >> 14;

   ---- End code block ----------------------------------------
*/

#include <stdio.h>
#include <string.h>

#include "bitstream_to_annexa.h"
#include <base/ovlibrary/ovlibrary.h>

#define OV_LOG_TAG "BitstreamAnnexA"

#   define AV_RL24(x)                           \
    ((((const uint8_t*)(x))[2] << 16) |         \
     (((const uint8_t*)(x))[1] <<  8) |         \
      ((const uint8_t*)(x))[0])

#   define AV_RL16(x)                           \
    ((((const uint8_t*)(x))[1] << 8) |          \
      ((const uint8_t*)(x))[0])


BitstreamAnnexA::BitstreamAnnexA()
{
}

BitstreamAnnexA::~BitstreamAnnexA()
{
}

void BitstreamAnnexA::convert_to(const std::shared_ptr<ov::Data> &data)
{
	uint8_t *p = data->GetWritableDataAs<uint8_t>();

	unsigned int frame_type;

	// +---------+-------------------------+-------------+
	// | Version | Reconstruction filter   | Loop filter |
	// +---------+-------------------------+-------------+
	// | 0       | Bicubic                 | Normal      |
	// | 1       | Bilinear                | Simple      |
	// | 2       | Bilinear                | None        |
	// | 3       | None                    | None        |
	// | Other   | Reserved for future use |             |
	// +---------+-------------------------+-------------+
	unsigned int profile;

	unsigned int sync_code;

	unsigned int width, height, horizontal_scale, vertical_scale;

	unsigned int show_frame;


	frame_type = p[0] & 0x01;
	profile = (p[0] >> 1) & 0x07;
	show_frame = (p[0] >> 4) & 0x01;

	if(frame_type == 0)
	{
		sync_code = AV_RL24(p + 3);

		width = AV_RL16(p + 6) & 0x3fff;
		horizontal_scale = AV_RL16(p + 6) >> 14;

		// 0011 1111 1111 1111
		// Scale + Height
		height = AV_RL16(p + 8) & 0x3fff;
		vertical_scale = AV_RL16(p + 8) >> 14;

		// printf("%s", ov::Data::Dump(_pkt->data, _pkt->size, 0, 64).CStr());

		logtd("frame_type=%d, profile=%d, sync_code=%d, width=%d(%d), height=%d(%d), show_frame=%d",
		  frame_type, profile, sync_code, width, horizontal_scale, height, vertical_scale, show_frame);
	}
}
