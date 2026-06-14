#!/bin/sh
./glslcompiler.elf -vs ../shader/video.vert.glsl -ps ../shader/yuv420p.frag.glsl -o ../shader/yuv420p.gsh
./glslcompiler.elf -vs ../shader/video.vert.glsl -ps ../shader/nv12.frag.glsl -o ../shader/nv12.gsh
