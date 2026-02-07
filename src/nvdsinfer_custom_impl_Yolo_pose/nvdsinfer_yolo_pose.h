/*
 * Copyright (c) 2024, NVIDIA CORPORATION. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Edited by Marcos Luciano
 * https://www.github.com/marcoslucianops
 */

#ifndef __NVDSINFER_YOLO_POSE_H__
#define __NVDSINFER_YOLO_POSE_H__

#include <stdint.h>
#include <string.h>

#define kNMS_THRESH 0.45
#define kCONF_THRESH 0.25

struct alignas(float) Detection {
    float bbox[4];  // x, y, w, h
    float conf;
    float class_id;
};

struct PoseKeypoint {
    float x;
    float y;
    float confidence;
};

// COCO 17 keypoints
#define NUM_KEYPOINTS 17

struct PoseDetection {
    float bbox[4];  // x, y, w, h
    float conf;
    PoseKeypoint keypoints[NUM_KEYPOINTS];
};

// COCO keypoint connections for skeleton
static const int SKELETON_CONNECTIONS[][2] = {
    {16, 14}, {14, 12}, {17, 15}, {15, 13}, {12, 13},
    {6, 12}, {7, 13}, {6, 7}, {6, 8}, {7, 9},
    {8, 10}, {9, 11}, {2, 3}, {1, 2}, {1, 3},
    {2, 4}, {3, 5}, {4, 6}, {5, 7}
};

#endif  // __NVDSINFER_YOLO_POSE_H__
