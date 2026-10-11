# VLC table provenance

../include/cavlc_tables.h contains only numeric VLC tables extracted from
FFmpeg n7.1 libavcodec/h264_cavlc.c:
https://github.com/FFmpeg/FFmpeg/blob/n7.1/libavcodec/h264_cavlc.c

Copyright (c) 2003 Michael Niedermayer. These extracted tables retain the
LGPL-2.1-or-later license; see COPYING.LGPLv2.1. The encoder algorithm is implemented
separately. Tables correspond to H.264 residual CAVLC syntax (project PDF 10.2).
