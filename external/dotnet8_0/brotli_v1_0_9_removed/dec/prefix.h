/* Copyright 2013 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

/* Lookup tables to map prefix codes to value ranges. This is used during
   decoding of the block lengths, literal insertion lengths and copy lengths. */

#ifndef BROTLI_DEC_PREFIX_H_
#define BROTLI_DEC_PREFIX_H_

#include "../common/constants.h"
#include <brotli/types.h>

typedef struct CmdLutElement {
  uint8_t insert_len_extra_bits;
  uint8_t copy_len_extra_bits;
  int8_t distance_code;
  uint8_t context;
  uint16_t insert_len_offset;
  uint16_t copy_len_offset;
} CmdLutElement;

static const CmdLutElement kCmdLut[BROTLI_NUM_COMMAND_SYMBOLS] = {
  { 0x00, 0x00, 0, 0x00, 0x0000, 0x0002 },
  { 0x00, 0x00, 0, 0x01, 0x0000, 0x0003 },
  { 0x00, 0x00, 0, 0x02, 0x0000, 0x0004 },
  { 0x00, 0x00, 0, 0x03, 0x0000, 0x0005 },
  { 0x00, 0x00, 0, 0x03, 0x0000, 0x0006 },
  { 0x00, 0x00, 0, 0x03, 0x0000, 0x0007 },
  { 0x00, 0x00, 0, 0x03, 0x0000, 0x0008 },
  { 0x00, 0x00, 0, 0x03, 0x0000, 0x0009 },
  { 0x00, 0x00, 0, 0x00, 0x0001, 0x0002 },
  { 0x00, 0x00, 0, 0x01, 0x0001, 0x0003 },
  { 0x00, 0x00, 0, 0x02, 0x0001, 0x0004 },
  { 0x00, 0x00, 0, 0x03, 0x0001, 0x0005 },
  { 0x00, 0x00, 0, 0x03, 0x0001, 0x0006 },
  { 0x00, 0x00, 0, 0x03, 0x0001, 0x0007 },
  { 0x00, 0x00, 0, 0x03, 0x0001, 0x0008 },
  { 0x00, 0x00, 0, 0x03, 0x0001, 0x0009 },
  { 0x00, 0x00, 0, 0x00, 0x0002, 0x0002 },
  { 0x00, 0x00, 0, 0x01, 0x0002, 0x0003 },
  { 0x00, 0x00, 0, 0x02, 0x0002, 0x0004 },
  { 0x00, 0x00, 0, 0x03, 0x0002, 0x0005 },
  { 0x00, 0x00, 0, 0x03, 0x0002, 0x0006 },
  { 0x00, 0x00, 0, 0x03, 0x0002, 0x0007 },
  { 0x00, 0x00, 0, 0x03, 0x0002, 0x0008 },
  { 0x00, 0x00, 0, 0x03, 0x0002, 0x0009 },
  { 0x00, 0x00, 0, 0x00, 0x0003, 0x0002 },
  { 0x00, 0x00, 0, 0x01, 0x0003, 0x0003 },
  { 0x00, 0x00, 0, 0x02, 0x0003, 0x0004 },
  { 0x00, 0x00, 0, 0x03, 0x0003, 0x0005 },
  { 0x00, 0x00, 0, 0x03, 0x0003, 0x0006 },
  { 0x00, 0x00, 0, 0x03, 0x0003, 0x0007 },
  { 0x00, 0x00, 0, 0x03, 0x0003, 0x0008 },
  { 0x00, 0x00, 0, 0x03, 0x0003, 0x0009 },
  { 0x00, 0x00, 0, 0x00, 0x0004, 0x0002 },
  { 0x00, 0x00, 0, 0x01, 0x0004, 0x0003 },
  { 0x00, 0x00, 0, 0x02, 0x0004, 0x0004 },
  { 0x00, 0x00, 0, 0x03, 0x0004, 0x0005 },
  { 0x00, 0x00, 0, 0x03, 0x0004, 0x0006 },
  { 0x00, 0x00, 0, 0x03, 0x0004, 0x0007 },
  { 0x00, 0x00, 0, 0x03, 0x0004, 0x0008 },
  { 0x00, 0x00, 0, 0x03, 0x0004, 0x0009 },
  { 0x00, 0x00, 0, 0x00, 0x0005, 0x0002 },
  { 0x00, 0x00, 0, 0x01, 0x0005, 0x0003 },
  { 0x00, 0x00, 0, 0x02, 0x0005, 0x0004 },
  { 0x00, 0x00, 0, 0x03, 0x0005, 0x0005 },
  { 0x00, 0x00, 0, 0x03, 0x0005, 0x0006 },
  { 0x00, 0x00, 0, 0x03, 0x0005, 0x0007 },
  { 0x00, 0x00, 0, 0x03, 0x0005, 0x0008 },
  { 0x00, 0x00, 0, 0x03, 0x0005, 0x0009 },
  { 0x01, 0x00, 0, 0x00, 0x0006, 0x0002 },
  { 0x01, 0x00, 0, 0x01, 0x0006, 0x0003 },
  { 0x01, 0x00, 0, 0x02, 0x0006, 0x0004 },
  { 0x01, 0x00, 0, 0x03, 0x0006, 0x0005 },
  { 0x01, 0x00, 0, 0x03, 0x0006, 0x0006 },
  { 0x01, 0x00, 0, 0x03, 0x0006, 0x0007 },
  { 0x01, 0x00, 0, 0x03, 0x0006, 0x0008 },
  { 0x01, 0x00, 0, 0x03, 0x0006, 0x0009 },
  { 0x01, 0x00, 0, 0x00, 0x0008, 0x0002 },
  { 0x01, 0x00, 0, 0x01, 0x0008, 0x0003 },
  { 0x01, 0x00, 0, 0x02, 0x0008, 0x0004 },
  { 0x01, 0x00, 0, 0x03, 0x0008, 0x0005 },
  { 0x01, 0x00, 0, 0x03, 0x0008, 0x0006 },
  { 0x01, 0x00, 0, 0x03, 0x0008, 0x0007 },
  { 0x01, 0x00, 0, 0x03, 0x0008, 0x0008 },
  { 0x01, 0x00, 0, 0x03, 0x0008, 0x0009 },
  { 0x00, 0x01, 0, 0x03, 0x0000, 0x000a },
  { 0x00, 0x01, 0, 0x03, 0x0000, 0x000c },
  { 0x00, 0x02, 0, 0x03, 0x0000, 0x000e },
  { 0x00, 0x02, 0, 0x03, 0x0000, 0x0012 },
  { 0x00, 0x03, 0, 0x03, 0x0000, 0x0016 },
  { 0x00, 0x03, 0, 0x03, 0x0000, 0x001e },
  { 0x00, 0x04, 0, 0x03, 0x0000, 0x0026 },
  { 0x00, 0x04, 0, 0x03, 0x0000, 0x0036 },
  { 0x00, 0x01, 0, 0x03, 0x0001, 0x000a },
  { 0x00, 0x01, 0, 0x03, 0x0001, 0x000c },
  { 0x00, 0x02, 0, 0x03, 0x0001, 0x000e },
  { 0x00, 0x02, 0, 0x03, 0x0001, 0x0012 },
  { 0x00, 0x03, 0, 0x03, 0x0001, 0x0016 },
  { 0x00, 0x03, 0, 0x03, 0x0001, 0x001e },
  { 0x00, 0x04, 0, 0x03, 0x0001, 0x0026 },
  { 0x00, 0x04, 0, 0x03, 0x0001, 0x0036 },
  { 0x00, 0x01, 0, 0x03, 0x0002, 0x000a },
  { 0x00, 0x01, 0, 0x03, 0x0002, 0x000c },
  { 0x00, 0x02, 0, 0x03, 0x0002, 0x000e },
  { 0x00, 0x02, 0, 0x03, 0x0002, 0x0012 },
  { 0x00, 0x03, 0, 0x03, 0x0002, 0x0016 },
  { 0x00, 0x03, 0, 0x03, 0x0002, 0x001e },
  { 0x00, 0x04, 0, 0x03, 0x0002, 0x0026 },
  { 0x00, 0x04, 0, 0x03, 0x0002, 0x0036 },
  { 0x00, 0x01, 0, 0x03, 0x0003, 0x000a },
  { 0x00, 0x01, 0, 0x03, 0x0003, 0x000c },
  { 0x00, 0x02, 0, 0x03, 0x0003, 0x000e },
  { 0x00, 0x02, 0, 0x03, 0x0003, 0x0012 },
  { 0x00, 0x03, 0, 0x03, 0x0003, 0x0016 },
  { 0x00, 0x03, 0, 0x03, 0x0003, 0x001e },
  { 0x00, 0x04, 0, 0x03, 0x0003, 0x0026 },
  { 0x00, 0x04, 0, 0x03, 0x0003, 0x0036 },
  { 0x00, 0x01, 0, 0x03, 0x0004, 0x000a },
  { 0x00, 0x01, 0, 0x03, 0x0004, 0x000c },
  { 0x00, 0x02, 0, 0x03, 0x0004, 0x000e },
  { 0x00, 0x02, 0, 0x03, 0x0004, 0x0012 },
  { 0x00, 0x03, 0, 0x03, 0x0004, 0x0016 },
  { 0x00, 0x03, 0, 0x03, 0x0004, 0x001e },
  { 0x00, 0x04, 0, 0x03, 0x0004, 0x0026 },
  { 0x00, 0x04, 0, 0x03, 0x0004, 0x0036 },
  { 0x00, 0x01, 0, 0x03, 0x0005, 0x000a },
  { 0x00, 0x01, 0, 0x03, 0x0005, 0x000c },
  { 0x00, 0x02, 0, 0x03, 0x0005, 0x000e },
  { 0x00, 0x02, 0, 0x03, 0x0005, 0x0012 },
  { 0x00, 0x03, 0, 0x03, 0x0005, 0x0016 },
  { 0x00, 0x03, 0, 0x03, 0x0005, 0x001e },
  { 0x00, 0x04, 0, 0x03, 0x0005, 0x0026 },
  { 0x00, 0x04, 0, 0x03, 0x0005, 0x0036 },
  { 0x01, 0x01, 0, 0x03, 0x0006, 0x000a },
  { 0x01, 0x01, 0, 0x03, 0x0006, 0x000c },
  { 0x01, 0x02, 0, 0x03, 0x0006, 0x000e },
  { 0x01, 0x02, 0, 0x03, 0x0006, 0x0012 },
  { 0x01, 0x03, 0, 0x03, 0x0006, 0x0016 },
  { 0x01, 0x03, 0, 0x03, 0x0006, 0x001e },
  { 0x01, 0x04, 0, 0x03, 0x0006, 0x0026 },
  { 0x01, 0x04, 0, 0x03, 0x0006, 0x0036 },
  { 0x01, 0x01, 0, 0x03, 0x0008, 0x000a },
  { 0x01, 0x01, 0, 0x03, 0x0008, 0x000c },
  { 0x01, 0x02, 0, 0x03, 0x0008, 0x000e },
  { 0x01, 0x02, 0, 0x03, 0x0008, 0x0012 },
  { 0x01, 0x03, 0, 0x03, 0x0008, 0x0016 },
  { 0x01, 0x03, 0, 0x03, 0x0008, 0x001e },
  { 0x01, 0x04, 0, 0x03, 0x0008, 0x0026 },
  { 0x01, 0x04, 0, 0x03, 0x0008, 0x0036 },
  { 0x00, 0x00, -1, 0x00, 0x0000, 0x0002 },
  { 0x00, 0x00, -1, 0x01, 0x0000, 0x0003 },
  { 0x00, 0x00, -1, 0x02, 0x0000, 0x0004 },
  { 0x00, 0x00, -1, 0x03, 0x0000, 0x0005 },
  { 0x00, 0x00, -1, 0x03, 0x0000, 0x0006 },
  { 0x00, 0x00, -1, 0x03, 0x0000, 0x0007 },
  { 0x00, 0x00, -1, 0x03, 0x0000, 0x0008 },
  { 0x00, 0x00, -1, 0x03, 0x0000, 0x0009 },
  { 0x00, 0x00, -1, 0x00, 0x0001, 0x0002 },
  { 0x00, 0x00, -1, 0x01, 0x0001, 0x0003 },
  { 0x00, 0x00, -1, 0x02, 0x0001, 0x0004 },
  { 0x00, 0x00, -1, 0x03, 0x0001, 0x0005 },
  { 0x00, 0x00, -1, 0x03, 0x0001, 0x0006 },
  { 0x00, 0x00, -1, 0x03, 0x0001, 0x0007 },
  { 0x00, 0x00, -1, 0x03, 0x0001, 0x0008 },
  { 0x00, 0x00, -1, 0x03, 0x0001, 0x0009 },
  { 0x00, 0x00, -1, 0x00, 0x0002, 0x0002 },
  { 0x00, 0x00, -1, 0x01, 0x0002, 0x0003 },
  { 0x00, 0x00, -1, 0x02, 0x0002, 0x0004 },
  { 0x00, 0x00, -1, 0x03, 0x0002, 0x0005 },
  { 0x00, 0x00, -1, 0x03, 0x0002, 0x0006 },
  { 0x00, 0x00, -1, 0x03, 0x0002, 0x0007 },
  { 0x00, 0x00, -1, 0x03, 0x0002, 0x0008 },
  { 0x00, 0x00, -1, 0x03, 0x0002, 0x0009 },
  { 0x00, 0x00, -1, 0x00, 0x0003, 0x0002 },
  { 0x00, 0x00, -1, 0x01, 0x0003, 0x0003 },
  { 0x00, 0x00, -1, 0x02, 0x0003, 0x0004 },
  { 0x00, 0x00, -1, 0x03, 0x0003, 0x0005 },
  { 0x00, 0x00, -1, 0x03, 0x0003, 0x0006 },
  { 0x00, 0x00, -1, 0x03, 0x0003, 0x0007 },
  { 0x00, 0x00, -1, 0x03, 0x0003, 0x0008 },
  { 0x00, 0x00, -1, 0x03, 0x0003, 0x0009 },
  { 0x00, 0x00, -1, 0x00, 0x0004, 0x0002 },
  { 0x00, 0x00, -1, 0x01, 0x0004, 0x0003 },
  { 0x00, 0x00, -1, 0x02, 0x0004, 0x0004 },
  { 0x00, 0x00, -1, 0x03, 0x0004, 0x0005 },
  { 0x00, 0x00, -1, 0x03, 0x0004, 0x0006 },
  { 0x00, 0x00, -1, 0x03, 0x0004, 0x0007 },
  { 0x00, 0x00, -1, 0x03, 0x0004, 0x0008 },
  { 0x00, 0x00, -1, 0x03, 0x0004, 0x0009 },
  { 0x00, 0x00, -1, 0x00, 0x0005, 0x0002 },
  { 0x00, 0x00, -1, 0x01, 0x0005, 0x0003 },
  { 0x00, 0x00, -1, 0x02, 0x0005, 0x0004 },
  { 0x00, 0x00, -1, 0x03, 0x0005, 0x0005 },
  { 0x00, 0x00, -1, 0x03, 0x0005, 0x0006 },
  { 0x00, 0x00, -1, 0x03, 0x0005, 0x0007 },
  { 0x00, 0x00, -1, 0x03, 0x0005, 0x0008 },
  { 0x00, 0x00, -1, 0x03, 0x0005, 0x0009 },
  { 0x01, 0x00, -1, 0x00, 0x0006, 0x0002 },
  { 0x01, 0x00, -1, 0x01, 0x0006, 0x0003 },
  { 0x01, 0x00, -1, 0x02, 0x0006, 0x0004 },
  { 0x01, 0x00, -1, 0x03, 0x0006, 0x0005 },
  { 0x01, 0x00, -1, 0x03, 0x0006, 0x0006 },
  { 0x01, 0x00, -1, 0x03, 0x0006, 0x0007 },
  { 0x01, 0x00, -1, 0x03, 0x0006, 0x0008 },
  { 0x01, 0x00, -1, 0x03, 0x0006, 0x0009 },
  { 0x01, 0x00, -1, 0x00, 0x0008, 0x0002 },
  { 0x01, 0x00, -1, 0x01, 0x0008, 0x0003 },
  { 0x01, 0x00, -1, 0x02, 0x0008, 0x0004 },
  { 0x01, 0x00, -1, 0x03, 0x0008, 0x0005 },
  { 0x01, 0x00, -1, 0x03, 0x0008, 0x0006 },
  { 0x01, 0x00, -1, 0x03, 0x0008, 0x0007 },
  { 0x01, 0x00, -1, 0x03, 0x0008, 0x0008 },
  { 0x01, 0x00, -1, 0x03, 0x0008, 0x0009 },
  { 0x00, 0x01, -1, 0x03, 0x0000, 0x000a },
  { 0x00, 0x01, -1, 0x03, 0x0000, 0x000c },
  { 0x00, 0x02, -1, 0x03, 0x0000, 0x000e },
  { 0x00, 0x02, -1, 0x03, 0x0000, 0x0012 },
  { 0x00, 0x03, -1, 0x03, 0x0000, 0x0016 },
  { 0x00, 0x03, -1, 0x03, 0x0000, 0x001e },
  { 0x00, 0x04, -1, 0x03, 0x0000, 0x0026 },
  { 0x00, 0x04, -1, 0x03, 0x0000, 0x0036 },
  { 0x00, 0x01, -1, 0x03, 0x0001, 0x000a },
  { 0x00, 0x01, -1, 0x03, 0x0001, 0x000c },
  { 0x00, 0x02, -1, 0x03, 0x0001, 0x000e },
  { 0x00, 0x02, -1, 0x03, 0x0001, 0x0012 },
  { 0x00, 0x03, -1, 0x03, 0x0001, 0x0016 },
  { 0x00, 0x03, -1, 0x03, 0x0001, 0x001e },
  { 0x00, 0x04, -1, 0x03, 0x0001, 0x0026 },
  { 0x00, 0x04, -1, 0x03, 0x0001, 0x0036 },
  { 0x00, 0x01, -1, 0x03, 0x0002, 0x000a },
  { 0x00, 0x01, -1, 0x03, 0x0002, 0x000c },
  { 0x00, 0x02, -1, 0x03, 0x0002, 0x000e },
  { 0x00, 0x02, -1, 0x03, 0x0002, 0x0012 },
  { 0x00, 0x03, -1, 0x03, 0x0002, 0x0016 },
  { 0x00, 0x03, -1, 0x03, 0x0002, 0x001e },
  { 0x00, 0x04, -1, 0x03, 0x0002, 0x0026 },
  { 0x00, 0x04, -1, 0x03, 0x0002, 0x0036 },
  { 0x00, 0x01, -1, 0x03, 0x0003, 0x000a },
  { 0x00, 0x01, -1, 0x03, 0x0003, 0x000c },
  { 0x00, 0x02, -1, 0x03, 0x0003, 0x000e },
  { 0x00, 0x02, -1, 0x03, 0x0003, 0x0012 },
  { 0x00, 0x03, -1, 0x03, 0x0003, 0x0016 },
  { 0x00, 0x03, -1, 0x03, 0x0003, 0x001e },
  { 0x00, 0x04, -1, 0x03, 0x0003, 0x0026 },
  { 0x00, 0x04, -1, 0x03, 0x0003, 0x0036 },
  { 0x00, 0x01, -1, 0x03, 0x0004, 0x000a },
  { 0x00, 0x01, -1, 0x03, 0x0004, 0x000c },
  { 0x00, 0x02, -1, 0x03, 0x0004, 0x000e },
  { 0x00, 0x02, -1, 0x03, 0x0004, 0x0012 },
  { 0x00, 0x03, -1, 0x03, 0x0004, 0x0016 },
  { 0x00, 0x03, -1, 0x03, 0x0004, 0x001e },
  { 0x00, 0x04, -1, 0x03, 0x0004, 0x0026 },
  { 0x00, 0x04, -1, 0x03, 0x0004, 0x0036 },
  { 0x00, 0x01, -1, 0x03, 0x0005, 0x000a },
  { 0x00, 0x01, -1, 0x03, 0x0005, 0x000c },
  { 0x00, 0x02, -1, 0x03, 0x0005, 0x000e },
  { 0x00, 0x02, -1, 0x03, 0x0005, 0x0012 },
  { 0x00, 0x03, -1, 0x03, 0x0005, 0x0016 },
  { 0x00, 0x03, -1, 0x03, 0x0005, 0x001e },
  { 0x00, 0x04, -1, 0x03, 0x0005, 0x0026 },
  { 0x00, 0x04, -1, 0x03, 0x0005, 0x0036 },
  { 0x01, 0x01, -1, 0x03, 0x0006, 0x000a },
  { 0x01, 0x01, -1, 0x03, 0x0006, 0x000c },
  { 0x01, 0x02, -1, 0x03, 0x0006, 0x000e },
  { 0x01, 0x02, -1, 0x03, 0x0006, 0x0012 },
  { 0x01, 0x03, -1, 0x03, 0x0006, 0x0016 },
  { 0x01, 0x03, -1, 0x03, 0x0006, 0x001e },
  { 0x01, 0x04, -1, 0x03, 0x0006, 0x0026 },
  { 0x01, 0x04, -1, 0x03, 0x0006, 0x0036 },
  { 0x01, 0x01, -1, 0x03, 0x0008, 0x000a },
  { 0x01, 0x01, -1, 0x03, 0x0008, 0x000c },
  { 0x01, 0x02, -1, 0x03, 0x0008, 0x000e },
  { 0x01, 0x02, -1, 0x03, 0x0008, 0x0012 },
  { 0x01, 0x03, -1, 0x03, 0x0008, 0x0016 },
  { 0x01, 0x03, -1, 0x03, 0x0008, 0x001e },
  { 0x01, 0x04, -1, 0x03, 0x0008, 0x0026 },
  { 0x01, 0x04, -1, 0x03, 0x0008, 0x0036 },
  { 0x02, 0x00, -1, 0x00, 0x000a, 0x0002 },
  { 0x02, 0x00, -1, 0x01, 0x000a, 0x0003 },
  { 0x02, 0x00, -1, 0x02, 0x000a, 0x0004 },
  { 0x02, 0x00, -1, 0x03, 0x000a, 0x0005 },
  { 0x02, 0x00, -1, 0x03, 0x000a, 0x0006 },
  { 0x02, 0x00, -1, 0x03, 0x000a, 0x0007 },
  { 0x02, 0x00, -1, 0x03, 0x000a, 0x0008 },
  { 0x02, 0x00, -1, 0x03, 0x000a, 0x0009 },
  { 0x02, 0x00, -1, 0x00, 0x000e, 0x0002 },
  { 0x02, 0x00, -1, 0x01, 0x000e, 0x0003 },
  { 0x02, 0x00, -1, 0x02, 0x000e, 0x0004 },
  { 0x02, 0x00, -1, 0x03, 0x000e, 0x0005 },
  { 0x02, 0x00, -1, 0x03, 0x000e, 0x0006 },
  { 0x02, 0x00, -1, 0x03, 0x000e, 0x0007 },
  { 0x02, 0x00, -1, 0x03, 0x000e, 0x0008 },
  { 0x02, 0x00, -1, 0x03, 0x000e, 0x0009 },
  { 0x03, 0x00, -1, 0x00, 0x0012, 0x0002 },
  { 0x03, 0x00, -1, 0x01, 0x0012, 0x0003 },
  { 0x03, 0x00, -1, 0x02, 0x0012, 0x0004 },
  { 0x03, 0x00, -1, 0x03, 0x0012, 0x0005 },
  { 0x03, 0x00, -1, 0x03, 0x0012, 0x0006 },
  { 0x03, 0x00, -1, 0x03, 0x0012, 0x0007 },
  { 0x03, 0x00, -1, 0x03, 0x0012, 0x0008 },
  { 0x03, 0x00, -1, 0x03, 0x0012, 0x0009 },
  { 0x03, 0x00, -1, 0x00, 0x001a, 0x0002 },
  { 0x03, 0x00, -1, 0x01, 0x001a, 0x0003 },
  { 0x03, 0x00, -1, 0x02, 0x001a, 0x0004 },
  { 0x03, 0x00, -1, 0x03, 0x001a, 0x0005 },
  { 0x03, 0x00, -1, 0x03, 0x001a, 0x0006 },
  { 0x03, 0x00, -1, 0x03, 0x001a, 0x0007 },
  { 0x03, 0x00, -1, 0x03, 0x001a, 0x0008 },
  { 0x03, 0x00, -1, 0x03, 0x001a, 0x0009 },
  { 0x04, 0x00, -1, 0x00, 0x0022, 0x0002 },
  { 0x04, 0x00, -1, 0x01, 0x0022, 0x0003 },
  { 0x04, 0x00, -1, 0x02, 0x0022, 0x0004 },
  { 0x04, 0x00, -1, 0x03, 0x0022, 0x0005 },
  { 0x04, 0x00, -1, 0x03, 0x0022, 0x0006 },
  { 0x04, 0x00, -1, 0x03, 0x0022, 0x0007 },
  { 0x04, 0x00, -1, 0x03, 0x0022, 0x0008 },
  { 0x04, 0x00, -1, 0x03, 0x0022, 0x0009 },
  { 0x04, 0x00, -1, 0x00, 0x0032, 0x0002 },
  { 0x04, 0x00, -1, 0x01, 0x0032, 0x0003 },
  { 0x04, 0x00, -1, 0x02, 0x0032, 0x0004 },
  { 0x04, 0x00, -1, 0x03, 0x0032, 0x0005 },
  { 0x04, 0x00, -1, 0x03, 0x0032, 0x0006 },
  { 0x04, 0x00, -1, 0x03, 0x0032, 0x0007 },
  { 0x04, 0x00, -1, 0x03, 0x0032, 0x0008 },
  { 0x04, 0x00, -1, 0x03, 0x0032, 0x0009 },
  { 0x05, 0x00, -1, 0x00, 0x0042, 0x0002 },
  { 0x05, 0x00, -1, 0x01, 0x0042, 0x0003 },
  { 0x05, 0x00, -1, 0x02, 0x0042, 0x0004 },
  { 0x05, 0x00, -1, 0x03, 0x0042, 0x0005 },
  { 0x05, 0x00, -1, 0x03, 0x0042, 0x0006 },
  { 0x05, 0x00, -1, 0x03, 0x0042, 0x0007 },
  { 0x05, 0x00, -1, 0x03, 0x0042, 0x0008 },
  { 0x05, 0x00, -1, 0x03, 0x0042, 0x0009 },
  { 0x05, 0x00, -1, 0x00, 0x0062, 0x0002 },
  { 0x05, 0x00, -1, 0x01, 0x0062, 0x0003 },
  { 0x05, 0x00, -1, 0x02, 0x0062, 0x0004 },
  { 0x05, 0x00, -1, 0x03, 0x0062, 0x0005 },
  { 0x05, 0x00, -1, 0x03, 0x0062, 0x0006 },
  { 0x05, 0x00, -1, 0x03, 0x0062, 0x0007 },
  { 0x05, 0x00, -1, 0x03, 0x0062, 0x0008 },
  { 0x05, 0x00, -1, 0x03, 0x0062, 0x0009 },
  { 0x02, 0x01, -1, 0x03, 0x000a, 0x000a },
  { 0x02, 0x01, -1, 0x03, 0x000a, 0x000c },
  { 0x02, 0x02, -1, 0x03, 0x000a, 0x000e },
  { 0x02, 0x02, -1, 0x03, 0x000a, 0x0012 },
  { 0x02, 0x03, -1, 0x03, 0x000a, 0x0016 },
  { 0x02, 0x03, -1, 0x03, 0x000a, 0x001e },
  { 0x02, 0x04, -1, 0x03, 0x000a, 0x0026 },
  { 0x02, 0x04, -1, 0x03, 0x000a, 0x0036 },
  { 0x02, 0x01, -1, 0x03, 0x000e, 0x000a },
  { 0x02, 0x01, -1, 0x03, 0x000e, 0x000c },
  { 0x02, 0x02, -1, 0x03, 0x000e, 0x000e },
  { 0x02, 0x02, -1, 0x03, 0x000e, 0x0012 },
  { 0x02, 0x03, -1, 0x03, 0x000e, 0x0016 },
  { 0x02, 0x03, -1, 0x03, 0x000e, 0x001e },
  { 0x02, 0x04, -1, 0x03, 0x000e, 0x0026 },
  { 0x02, 0x04, -1, 0x03, 0x000e, 0x0036 },
  { 0x03, 0x01, -1, 0x03, 0x0012, 0x000a },
  { 0x03, 0x01, -1, 0x03, 0x0012, 0x000c },
  { 0x03, 0x02, -1, 0x03, 0x0012, 0x000e },
  { 0x03, 0x02, -1, 0x03, 0x0012, 0x0012 },
  { 0x03, 0x03, -1, 0x03, 0x0012, 0x0016 },
  { 0x03, 0x03, -1, 0x03, 0x0012, 0x001e },
  { 0x03, 0x04, -1, 0x03, 0x0012, 0x0026 },
  { 0x03, 0x04, -1, 0x03, 0x0012, 0x0036 },
  { 0x03, 0x01, -1, 0x03, 0x001a, 0x000a },
  { 0x03, 0x01, -1, 0x03, 0x001a, 0x000c },
  { 0x03, 0x02, -1, 0x03, 0x001a, 0x000e },
  { 0x03, 0x02, -1, 0x03, 0x001a, 0x0012 },
  { 0x03, 0x03, -1, 0x03, 0x001a, 0x0016 },
  { 0x03, 0x03, -1, 0x03, 0x001a, 0x001e },
  { 0x03, 0x04, -1, 0x03, 0x001a, 0x0026 },
  { 0x03, 0x04, -1, 0x03, 0x001a, 0x0036 },
  { 0x04, 0x01, -1, 0x03, 0x0022, 0x000a },
  { 0x04, 0x01, -1, 0x03, 0x0022, 0x000c },
  { 0x04, 0x02, -1, 0x03, 0x0022, 0x000e },
  { 0x04, 0x02, -1, 0x03, 0x0022, 0x0012 },
  { 0x04, 0x03, -1, 0x03, 0x0022, 0x0016 },
  { 0x04, 0x03, -1, 0x03, 0x0022, 0x001e },
  { 0x04, 0x04, -1, 0x03, 0x0022, 0x0026 },
  { 0x04, 0x04, -1, 0x03, 0x0022, 0x0036 },
  { 0x04, 0x01, -1, 0x03, 0x0032, 0x000a },
  { 0x04, 0x01, -1, 0x03, 0x0032, 0x000c },
  { 0x04, 0x02, -1, 0x03, 0x0032, 0x000e },
  { 0x04, 0x02, -1, 0x03, 0x0032, 0x0012 },
  { 0x04, 0x03, -1, 0x03, 0x0032, 0x0016 },
  { 0x04, 0x03, -1, 0x03, 0x0032, 0x001e },
  { 0x04, 0x04, -1, 0x03, 0x0032, 0x0026 },
  { 0x04, 0x04, -1, 0x03, 0x0032, 0x0036 },
  { 0x05, 0x01, -1, 0x03, 0x0042, 0x000a },
  { 0x05, 0x01, -1, 0x03, 0x0042, 0x000c },
  { 0x05, 0x02, -1, 0x03, 0x0042, 0x000e },
  { 0x05, 0x02, -1, 0x03, 0x0042, 0x0012 },
  { 0x05, 0x03, -1, 0x03, 0x0042, 0x0016 },
  { 0x05, 0x03, -1, 0x03, 0x0042, 0x001e },
  { 0x05, 0x04, -1, 0x03, 0x0042, 0x0026 },
  { 0x05, 0x04, -1, 0x03, 0x0042, 0x0036 },
  { 0x05, 0x01, -1, 0x03, 0x0062, 0x000a },
  { 0x05, 0x01, -1, 0x03, 0x0062, 0x000c },
  { 0x05, 0x02, -1, 0x03, 0x0062, 0x000e },
  { 0x05, 0x02, -1, 0x03, 0x0062, 0x0012 },
  { 0x05, 0x03, -1, 0x03, 0x0062, 0x0016 },
  { 0x05, 0x03, -1, 0x03, 0x0062, 0x001e },
  { 0x05, 0x04, -1, 0x03, 0x0062, 0x0026 },
  { 0x05, 0x04, -1, 0x03, 0x0062, 0x0036 },
  { 0x00, 0x05, -1, 0x03, 0x0000, 0x0046 },
  { 0x00, 0x05, -1, 0x03, 0x0000, 0x0066 },
  { 0x00, 0x06, -1, 0x03, 0x0000, 0x0086 },
  { 0x00, 0x07, -1, 0x03, 0x0000, 0x00c6 },
  { 0x00, 0x08, -1, 0x03, 0x0000, 0x0146 },
  { 0x00, 0x09, -1, 0x03, 0x0000, 0x0246 },
  { 0x00, 0x0a, -1, 0x03, 0x0000, 0x0446 },
  { 0x00, 0x18, -1, 0x03, 0x0000, 0x0846 },
  { 0x00, 0x05, -1, 0x03, 0x0001, 0x0046 },
  { 0x00, 0x05, -1, 0x03, 0x0001, 0x0066 },
  { 0x00, 0x06, -1, 0x03, 0x0001, 0x0086 },
  { 0x00, 0x07, -1, 0x03, 0x0001, 0x00c6 },
  { 0x00, 0x08, -1, 0x03, 0x0001, 0x0146 },
  { 0x00, 0x09, -1, 0x03, 0x0001, 0x0246 },
  { 0x00, 0x0a, -1, 0x03, 0x0001, 0x0446 },
  { 0x00, 0x18, -1, 0x03, 0x0001, 0x0846 },
  { 0x00, 0x05, -1, 0x03, 0x0002, 0x0046 },
  { 0x00, 0x05, -1, 0x03, 0x0002, 0x0066 },
  { 0x00, 0x06, -1, 0x03, 0x0002, 0x0086 },
  { 0x00, 0x07, -1, 0x03, 0x0002, 0x00c6 },
  { 0x00, 0x08, -1, 0x03, 0x0002, 0x0146 },
  { 0x00, 0x09, -1, 0x03, 0x0002, 0x0246 },
  { 0x00, 0x0a, -1, 0x03, 0x0002, 0x0446 },
  { 0x00, 0x18, -1, 0x03, 0x0002, 0x0846 },
  { 0x00, 0x05, -1, 0x03, 0x0003, 0x0046 },
  { 0x00, 0x05, -1, 0x03, 0x0003, 0x0066 },
  { 0x00, 0x06, -1, 0x03, 0x0003, 0x0086 },
  { 0x00, 0x07, -1, 0x03, 0x0003, 0x00c6 },
  { 0x00, 0x08, -1, 0x03, 0x0003, 0x0146 },
  { 0x00, 0x09, -1, 0x03, 0x0003, 0x0246 },
  { 0x00, 0x0a, -1, 0x03, 0x0003, 0x0446 },
  { 0x00, 0x18, -1, 0x03, 0x0003, 0x0846 },
  { 0x00, 0x05, -1, 0x03, 0x0004, 0x0046 },
  { 0x00, 0x05, -1, 0x03, 0x0004, 0x0066 },
  { 0x00, 0x06, -1, 0x03, 0x0004, 0x0086 },
  { 0x00, 0x07, -1, 0x03, 0x0004, 0x00c6 },
  { 0x00, 0x08, -1, 0x03, 0x0004, 0x0146 },
  { 0x00, 0x09, -1, 0x03, 0x0004, 0x0246 },
  { 0x00, 0x0a, -1, 0x03, 0x0004, 0x0446 },
  { 0x00, 0x18, -1, 0x03, 0x0004, 0x0846 },
  { 0x00, 0x05, -1, 0x03, 0x0005, 0x0046 },
  { 0x00, 0x05, -1, 0x03, 0x0005, 0x0066 },
  { 0x00, 0x06, -1, 0x03, 0x0005, 0x0086 },
  { 0x00, 0x07, -1, 0x03, 0x0005, 0x00c6 },
  { 0x00, 0x08, -1, 0x03, 0x0005, 0x0146 },
  { 0x00, 0x09, -1, 0x03, 0x0005, 0x0246 },
  { 0x00, 0x0a, -1, 0x03, 0x0005, 0x0446 },
  { 0x00, 0x18, -1, 0x03, 0x0005, 0x0846 },
  { 0x01, 0x05, -1, 0x03, 0x0006, 0x0046 },
  { 0x01, 0x05, -1, 0x03, 0x0006, 0x0066 },
  { 0x01, 0x06, -1, 0x03, 0x0006, 0x0086 },
  { 0x01, 0x07, -1, 0x03, 0x0006, 0x00c6 },
  { 0x01, 0x08, -1, 0x03, 0x0006, 0x0146 },
  { 0x01, 0x09, -1, 0x03, 0x0006, 0x0246 },
  { 0x01, 0x0a, -1, 0x03, 0x0006, 0x0446 },
  { 0x01, 0x18, -1, 0x03, 0x0006, 0x0846 },
  { 0x01, 0x05, -1, 0x03, 0x0008, 0x0046 },
  { 0x01, 0x05, -1, 0x03, 0x0008, 0x0066 },
  { 0x01, 0x06, -1, 0x03, 0x0008, 0x0086 },
  { 0x01, 0x07, -1, 0x03, 0x0008, 0x00c6 },
  { 0x01, 0x08, -1, 0x03, 0x0008, 0x0146 },
  { 0x01, 0x09, -1, 0x03, 0x0008, 0x0246 },
  { 0x01, 0x0a, -1, 0x03, 0x0008, 0x0446 },
  { 0x01, 0x18, -1, 0x03, 0x0008, 0x0846 },
  { 0x06, 0x00, -1, 0x00, 0x0082, 0x0002 },
  { 0x06, 0x00, -1, 0x01, 0x0082, 0x0003 },
  { 0x06, 0x00, -1, 0x02, 0x0082, 0x0004 },
  { 0x06, 0x00, -1, 0x03, 0x0082, 0x0005 },
  { 0x06, 0x00, -1, 0x03, 0x0082, 0x0006 },
  { 0x06, 0x00, -1, 0x03, 0x0082, 0x0007 },
  { 0x06, 0x00, -1, 0x03, 0x0082, 0x0008 },
  { 0x06, 0x00, -1, 0x03, 0x0082, 0x0009 },
  { 0x07, 0x00, -1, 0x00, 0x00c2, 0x0002 },
  { 0x07, 0x00, -1, 0x01, 0x00c2, 0x0003 },
  { 0x07, 0x00, -1, 0x02, 0x00c2, 0x0004 },
  { 0x07, 0x00, -1, 0x03, 0x00c2, 0x0005 },
  { 0x07, 0x00, -1, 0x03, 0x00c2, 0x0006 },
  { 0x07, 0x00, -1, 0x03, 0x00c2, 0x0007 },
  { 0x07, 0x00, -1, 0x03, 0x00c2, 0x0008 },
  { 0x07, 0x00, -1, 0x03, 0x00c2, 0x0009 },
  { 0x08, 0x00, -1, 0x00, 0x0142, 0x0002 },
  { 0x08, 0x00, -1, 0x01, 0x0142, 0x0003 },
  { 0x08, 0x00, -1, 0x02, 0x0142, 0x0004 },
  { 0x08, 0x00, -1, 0x03, 0x0142, 0x0005 },
  { 0x08, 0x00, -1, 0x03, 0x0142, 0x0006 },
  { 0x08, 0x00, -1, 0x03, 0x0142, 0x0007 },
  { 0x08, 0x00, -1, 0x03, 0x0142, 0x0008 },
  { 0x08, 0x00, -1, 0x03, 0x0142, 0x0009 },
  { 0x09, 0x00, -1, 0x00, 0x0242, 0x0002 },
  { 0x09, 0x00, -1, 0x01, 0x0242, 0x0003 },
  { 0x09, 0x00, -1, 0x02, 0x0242, 0x0004 },
  { 0x09, 0x00, -1, 0x03, 0x0242, 0x0005 },
  { 0x09, 0x00, -1, 0x03, 0x0242, 0x0006 },
  { 0x09, 0x00, -1, 0x03, 0x0242, 0x0007 },
  { 0x09, 0x00, -1, 0x03, 0x0242, 0x0008 },
  { 0x09, 0x00, -1, 0x03, 0x0242, 0x0009 },
  { 0x0a, 0x00, -1, 0x00, 0x0442, 0x0002 },
  { 0x0a, 0x00, -1, 0x01, 0x0442, 0x0003 },
  { 0x0a, 0x00, -1, 0x02, 0x0442, 0x0004 },
  { 0x0a, 0x00, -1, 0x03, 0x0442, 0x0005 },
  { 0x0a, 0x00, -1, 0x03, 0x0442, 0x0006 },
  { 0x0a, 0x00, -1, 0x03, 0x0442, 0x0007 },
  { 0x0a, 0x00, -1, 0x03, 0x0442, 0x0008 },
  { 0x0a, 0x00, -1, 0x03, 0x0442, 0x0009 },
  { 0x0c, 0x00, -1, 0x00, 0x0842, 0x0002 },
  { 0x0c, 0x00, -1, 0x01, 0x0842, 0x0003 },
  { 0x0c, 0x00, -1, 0x02, 0x0842, 0x0004 },
  { 0x0c, 0x00, -1, 0x03, 0x0842, 0x0005 },
  { 0x0c, 0x00, -1, 0x03, 0x0842, 0x0006 },
  { 0x0c, 0x00, -1, 0x03, 0x0842, 0x0007 },
  { 0x0c, 0x00, -1, 0x03, 0x0842, 0x0008 },
  { 0x0c, 0x00, -1, 0x03, 0x0842, 0x0009 },
  { 0x0e, 0x00, -1, 0x00, 0x1842, 0x0002 },
  { 0x0e, 0x00, -1, 0x01, 0x1842, 0x0003 },
  { 0x0e, 0x00, -1, 0x02, 0x1842, 0x0004 },
  { 0x0e, 0x00, -1, 0x03, 0x1842, 0x0005 },
  { 0x0e, 0x00, -1, 0x03, 0x1842, 0x0006 },
  { 0x0e, 0x00, -1, 0x03, 0x1842, 0x0007 },
  { 0x0e, 0x00, -1, 0x03, 0x1842, 0x0008 },
  { 0x0e, 0x00, -1, 0x03, 0x1842, 0x0009 },
  { 0x18, 0x00, -1, 0x00, 0x5842, 0x0002 },
  { 0x18, 0x00, -1, 0x01, 0x5842, 0x0003 },
  { 0x18, 0x00, -1, 0x02, 0x5842, 0x0004 },
  { 0x18, 0x00, -1, 0x03, 0x5842, 0x0005 },
  { 0x18, 0x00, -1, 0x03, 0x5842, 0x0006 },
  { 0x18, 0x00, -1, 0x03, 0x5842, 0x0007 },
  { 0x18, 0x00, -1, 0x03, 0x5842, 0x0008 },
  { 0x18, 0x00, -1, 0x03, 0x5842, 0x0009 },
  { 0x02, 0x05, -1, 0x03, 0x000a, 0x0046 },
  { 0x02, 0x05, -1, 0x03, 0x000a, 0x0066 },
  { 0x02, 0x06, -1, 0x03, 0x000a, 0x0086 },
  { 0x02, 0x07, -1, 0x03, 0x000a, 0x00c6 },
  { 0x02, 0x08, -1, 0x03, 0x000a, 0x0146 },
  { 0x02, 0x09, -1, 0x03, 0x000a, 0x0246 },
  { 0x02, 0x0a, -1, 0x03, 0x000a, 0x0446 },
  { 0x02, 0x18, -1, 0x03, 0x000a, 0x0846 },
  { 0x02, 0x05, -1, 0x03, 0x000e, 0x0046 },
  { 0x02, 0x05, -1, 0x03, 0x000e, 0x0066 },
  { 0x02, 0x06, -1, 0x03, 0x000e, 0x0086 },
  { 0x02, 0x07, -1, 0x03, 0x000e, 0x00c6 },
  { 0x02, 0x08, -1, 0x03, 0x000e, 0x0146 },
  { 0x02, 0x09, -1, 0x03, 0x000e, 0x0246 },
  { 0x02, 0x0a, -1, 0x03, 0x000e, 0x0446 },
  { 0x02, 0x18, -1, 0x03, 0x000e, 0x0846 },
  { 0x03, 0x05, -1, 0x03, 0x0012, 0x0046 },
  { 0x03, 0x05, -1, 0x03, 0x0012, 0x0066 },
  { 0x03, 0x06, -1, 0x03, 0x0012, 0x0086 },
  { 0x03, 0x07, -1, 0x03, 0x0012, 0x00c6 },
  { 0x03, 0x08, -1, 0x03, 0x0012, 0x0146 },
  { 0x03, 0x09, -1, 0x03, 0x0012, 0x0246 },
  { 0x03, 0x0a, -1, 0x03, 0x0012, 0x0446 },
  { 0x03, 0x18, -1, 0x03, 0x0012, 0x0846 },
  { 0x03, 0x05, -1, 0x03, 0x001a, 0x0046 },
  { 0x03, 0x05, -1, 0x03, 0x001a, 0x0066 },
  { 0x03, 0x06, -1, 0x03, 0x001a, 0x0086 },
  { 0x03, 0x07, -1, 0x03, 0x001a, 0x00c6 },
  { 0x03, 0x08, -1, 0x03, 0x001a, 0x0146 },
  { 0x03, 0x09, -1, 0x03, 0x001a, 0x0246 },
  { 0x03, 0x0a, -1, 0x03, 0x001a, 0x0446 },
  { 0x03, 0x18, -1, 0x03, 0x001a, 0x0846 },
  { 0x04, 0x05, -1, 0x03, 0x0022, 0x0046 },
  { 0x04, 0x05, -1, 0x03, 0x0022, 0x0066 },
  { 0x04, 0x06, -1, 0x03, 0x0022, 0x0086 },
  { 0x04, 0x07, -1, 0x03, 0x0022, 0x00c6 },
  { 0x04, 0x08, -1, 0x03, 0x0022, 0x0146 },
  { 0x04, 0x09, -1, 0x03, 0x0022, 0x0246 },
  { 0x04, 0x0a, -1, 0x03, 0x0022, 0x0446 },
  { 0x04, 0x18, -1, 0x03, 0x0022, 0x0846 },
  { 0x04, 0x05, -1, 0x03, 0x0032, 0x0046 },
  { 0x04, 0x05, -1, 0x03, 0x0032, 0x0066 },
  { 0x04, 0x06, -1, 0x03, 0x0032, 0x0086 },
  { 0x04, 0x07, -1, 0x03, 0x0032, 0x00c6 },
  { 0x04, 0x08, -1, 0x03, 0x0032, 0x0146 },
  { 0x04, 0x09, -1, 0x03, 0x0032, 0x0246 },
  { 0x04, 0x0a, -1, 0x03, 0x0032, 0x0446 },
  { 0x04, 0x18, -1, 0x03, 0x0032, 0x0846 },
  { 0x05, 0x05, -1, 0x03, 0x0042, 0x0046 },
  { 0x05, 0x05, -1, 0x03, 0x0042, 0x0066 },
  { 0x05, 0x06, -1, 0x03, 0x0042, 0x0086 },
  { 0x05, 0x07, -1, 0x03, 0x0042, 0x00c6 },
  { 0x05, 0x08, -1, 0x03, 0x0042, 0x0146 },
  { 0x05, 0x09, -1, 0x03, 0x0042, 0x0246 },
  { 0x05, 0x0a, -1, 0x03, 0x0042, 0x0446 },
  { 0x05, 0x18, -1, 0x03, 0x0042, 0x0846 },
  { 0x05, 0x05, -1, 0x03, 0x0062, 0x0046 },
  { 0x05, 0x05, -1, 0x03, 0x0062, 0x0066 },
  { 0x05, 0x06, -1, 0x03, 0x0062, 0x0086 },
  { 0x05, 0x07, -1, 0x03, 0x0062, 0x00c6 },
  { 0x05, 0x08, -1, 0x03, 0x0062, 0x0146 },
  { 0x05, 0x09, -1, 0x03, 0x0062, 0x0246 },
  { 0x05, 0x0a, -1, 0x03, 0x0062, 0x0446 },
  { 0x05, 0x18, -1, 0x03, 0x0062, 0x0846 },
  { 0x06, 0x01, -1, 0x03, 0x0082, 0x000a },
  { 0x06, 0x01, -1, 0x03, 0x0082, 0x000c },
  { 0x06, 0x02, -1, 0x03, 0x0082, 0x000e },
  { 0x06, 0x02, -1, 0x03, 0x0082, 0x0012 },
  { 0x06, 0x03, -1, 0x03, 0x0082, 0x0016 },
  { 0x06, 0x03, -1, 0x03, 0x0082, 0x001e },
  { 0x06, 0x04, -1, 0x03, 0x0082, 0x0026 },
  { 0x06, 0x04, -1, 0x03, 0x0082, 0x0036 },
  { 0x07, 0x01, -1, 0x03, 0x00c2, 0x000a },
  { 0x07, 0x01, -1, 0x03, 0x00c2, 0x000c },
  { 0x07, 0x02, -1, 0x03, 0x00c2, 0x000e },
  { 0x07, 0x02, -1, 0x03, 0x00c2, 0x0012 },
  { 0x07, 0x03, -1, 0x03, 0x00c2, 0x0016 },
  { 0x07, 0x03, -1, 0x03, 0x00c2, 0x001e },
  { 0x07, 0x04, -1, 0x03, 0x00c2, 0x0026 },
  { 0x07, 0x04, -1, 0x03, 0x00c2, 0x0036 },
  { 0x08, 0x01, -1, 0x03, 0x0142, 0x000a },
  { 0x08, 0x01, -1, 0x03, 0x0142, 0x000c },
  { 0x08, 0x02, -1, 0x03, 0x0142, 0x000e },
  { 0x08, 0x02, -1, 0x03, 0x0142, 0x0012 },
  { 0x08, 0x03, -1, 0x03, 0x0142, 0x0016 },
  { 0x08, 0x03, -1, 0x03, 0x0142, 0x001e },
  { 0x08, 0x04, -1, 0x03, 0x0142, 0x0026 },
  { 0x08, 0x04, -1, 0x03, 0x0142, 0x0036 },
  { 0x09, 0x01, -1, 0x03, 0x0242, 0x000a },
  { 0x09, 0x01, -1, 0x03, 0x0242, 0x000c },
  { 0x09, 0x02, -1, 0x03, 0x0242, 0x000e },
  { 0x09, 0x02, -1, 0x03, 0x0242, 0x0012 },
  { 0x09, 0x03, -1, 0x03, 0x0242, 0x0016 },
  { 0x09, 0x03, -1, 0x03, 0x0242, 0x001e },
  { 0x09, 0x04, -1, 0x03, 0x0242, 0x0026 },
  { 0x09, 0x04, -1, 0x03, 0x0242, 0x0036 },
  { 0x0a, 0x01, -1, 0x03, 0x0442, 0x000a },
  { 0x0a, 0x01, -1, 0x03, 0x0442, 0x000c },
  { 0x0a, 0x02, -1, 0x03, 0x0442, 0x000e },
  { 0x0a, 0x02, -1, 0x03, 0x0442, 0x0012 },
  { 0x0a, 0x03, -1, 0x03, 0x0442, 0x0016 },
  { 0x0a, 0x03, -1, 0x03, 0x0442, 0x001e },
  { 0x0a, 0x04, -1, 0x03, 0x0442, 0x0026 },
  { 0x0a, 0x04, -1, 0x03, 0x0442, 0x0036 },
  { 0x0c, 0x01, -1, 0x03, 0x0842, 0x000a },
  { 0x0c, 0x01, -1, 0x03, 0x0842, 0x000c },
  { 0x0c, 0x02, -1, 0x03, 0x0842, 0x000e },
  { 0x0c, 0x02, -1, 0x03, 0x0842, 0x0012 },
  { 0x0c, 0x03, -1, 0x03, 0x0842, 0x0016 },
  { 0x0c, 0x03, -1, 0x03, 0x0842, 0x001e },
  { 0x0c, 0x04, -1, 0x03, 0x0842, 0x0026 },
  { 0x0c, 0x04, -1, 0x03, 0x0842, 0x0036 },
  { 0x0e, 0x01, -1, 0x03, 0x1842, 0x000a },
  { 0x0e, 0x01, -1, 0x03, 0x1842, 0x000c },
  { 0x0e, 0x02, -1, 0x03, 0x1842, 0x000e },
  { 0x0e, 0x02, -1, 0x03, 0x1842, 0x0012 },
  { 0x0e, 0x03, -1, 0x03, 0x1842, 0x0016 },
  { 0x0e, 0x03, -1, 0x03, 0x1842, 0x001e },
  { 0x0e, 0x04, -1, 0x03, 0x1842, 0x0026 },
  { 0x0e, 0x04, -1, 0x03, 0x1842, 0x0036 },
  { 0x18, 0x01, -1, 0x03, 0x5842, 0x000a },
  { 0x18, 0x01, -1, 0x03, 0x5842, 0x000c },
  { 0x18, 0x02, -1, 0x03, 0x5842, 0x000e },
  { 0x18, 0x02, -1, 0x03, 0x5842, 0x0012 },
  { 0x18, 0x03, -1, 0x03, 0x5842, 0x0016 },
  { 0x18, 0x03, -1, 0x03, 0x5842, 0x001e },
  { 0x18, 0x04, -1, 0x03, 0x5842, 0x0026 },
  { 0x18, 0x04, -1, 0x03, 0x5842, 0x0036 },
  { 0x06, 0x05, -1, 0x03, 0x0082, 0x0046 },
  { 0x06, 0x05, -1, 0x03, 0x0082, 0x0066 },
  { 0x06, 0x06, -1, 0x03, 0x0082, 0x0086 },
  { 0x06, 0x07, -1, 0x03, 0x0082, 0x00c6 },
  { 0x06, 0x08, -1, 0x03, 0x0082, 0x0146 },
  { 0x06, 0x09, -1, 0x03, 0x0082, 0x0246 },
  { 0x06, 0x0a, -1, 0x03, 0x0082, 0x0446 },
  { 0x06, 0x18, -1, 0x03, 0x0082, 0x0846 },
  { 0x07, 0x05, -1, 0x03, 0x00c2, 0x0046 },
  { 0x07, 0x05, -1, 0x03, 0x00c2, 0x0066 },
  { 0x07, 0x06, -1, 0x03, 0x00c2, 0x0086 },
  { 0x07, 0x07, -1, 0x03, 0x00c2, 0x00c6 },
  { 0x07, 0x08, -1, 0x03, 0x00c2, 0x0146 },
  { 0x07, 0x09, -1, 0x03, 0x00c2, 0x0246 },
  { 0x07, 0x0a, -1, 0x03, 0x00c2, 0x0446 },
  { 0x07, 0x18, -1, 0x03, 0x00c2, 0x0846 },
  { 0x08, 0x05, -1, 0x03, 0x0142, 0x0046 },
  { 0x08, 0x05, -1, 0x03, 0x0142, 0x0066 },
  { 0x08, 0x06, -1, 0x03, 0x0142, 0x0086 },
  { 0x08, 0x07, -1, 0x03, 0x0142, 0x00c6 },
  { 0x08, 0x08, -1, 0x03, 0x0142, 0x0146 },
  { 0x08, 0x09, -1, 0x03, 0x0142, 0x0246 },
  { 0x08, 0x0a, -1, 0x03, 0x0142, 0x0446 },
  { 0x08, 0x18, -1, 0x03, 0x0142, 0x0846 },
  { 0x09, 0x05, -1, 0x03, 0x0242, 0x0046 },
  { 0x09, 0x05, -1, 0x03, 0x0242, 0x0066 },
  { 0x09, 0x06, -1, 0x03, 0x0242, 0x0086 },
  { 0x09, 0x07, -1, 0x03, 0x0242, 0x00c6 },
  { 0x09, 0x08, -1, 0x03, 0x0242, 0x0146 },
  { 0x09, 0x09, -1, 0x03, 0x0242, 0x0246 },
  { 0x09, 0x0a, -1, 0x03, 0x0242, 0x0446 },
  { 0x09, 0x18, -1, 0x03, 0x0242, 0x0846 },
  { 0x0a, 0x05, -1, 0x03, 0x0442, 0x0046 },
  { 0x0a, 0x05, -1, 0x03, 0x0442, 0x0066 },
  { 0x0a, 0x06, -1, 0x03, 0x0442, 0x0086 },
  { 0x0a, 0x07, -1, 0x03, 0x0442, 0x00c6 },
  { 0x0a, 0x08, -1, 0x03, 0x0442, 0x0146 },
  { 0x0a, 0x09, -1, 0x03, 0x0442, 0x0246 },
  { 0x0a, 0x0a, -1, 0x03, 0x0442, 0x0446 },
  { 0x0a, 0x18, -1, 0x03, 0x0442, 0x0846 },
  { 0x0c, 0x05, -1, 0x03, 0x0842, 0x0046 },
  { 0x0c, 0x05, -1, 0x03, 0x0842, 0x0066 },
  { 0x0c, 0x06, -1, 0x03, 0x0842, 0x0086 },
  { 0x0c, 0x07, -1, 0x03, 0x0842, 0x00c6 },
  { 0x0c, 0x08, -1, 0x03, 0x0842, 0x0146 },
  { 0x0c, 0x09, -1, 0x03, 0x0842, 0x0246 },
  { 0x0c, 0x0a, -1, 0x03, 0x0842, 0x0446 },
  { 0x0c, 0x18, -1, 0x03, 0x0842, 0x0846 },
  { 0x0e, 0x05, -1, 0x03, 0x1842, 0x0046 },
  { 0x0e, 0x05, -1, 0x03, 0x1842, 0x0066 },
  { 0x0e, 0x06, -1, 0x03, 0x1842, 0x0086 },
  { 0x0e, 0x07, -1, 0x03, 0x1842, 0x00c6 },
  { 0x0e, 0x08, -1, 0x03, 0x1842, 0x0146 },
  { 0x0e, 0x09, -1, 0x03, 0x1842, 0x0246 },
  { 0x0e, 0x0a, -1, 0x03, 0x1842, 0x0446 },
  { 0x0e, 0x18, -1, 0x03, 0x1842, 0x0846 },
  { 0x18, 0x05, -1, 0x03, 0x5842, 0x0046 },
  { 0x18, 0x05, -1, 0x03, 0x5842, 0x0066 },
  { 0x18, 0x06, -1, 0x03, 0x5842, 0x0086 },
  { 0x18, 0x07, -1, 0x03, 0x5842, 0x00c6 },
  { 0x18, 0x08, -1, 0x03, 0x5842, 0x0146 },
  { 0x18, 0x09, -1, 0x03, 0x5842, 0x0246 },
  { 0x18, 0x0a, -1, 0x03, 0x5842, 0x0446 },
  { 0x18, 0x18, -1, 0x03, 0x5842, 0x0846 },
};

#endif  /* BROTLI_DEC_PREFIX_H_ */
