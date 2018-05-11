/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - opencv_video_backend.h                                  *
 *   Mupen64Plus homepage: https://mupen64plus.org/                        *
 *   Copyright (C) 2017 Bobby Smiles                                       *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "opencv2/core/version.hpp"

#if CV_MAJOR_VERSION >= 3
/* this is for opencv >= 3.0 (new style headers + videoio/highgui split) */
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>
#elif CV_MAJOR_VERSION >= 2
/* otherwise go the safe way and let opencv include all its headers */
#include <opencv2/opencv.hpp>
#else
#error "Unsupported version of OpenCV"
#endif

#include <cstdlib>

extern "C"
{
#include "backends/opencv_video_backend.h"
#include "backends/api/video_backend.h"

#define M64P_CORE_PROTOTYPES 1
#include "api/callbacks.h"
#include "api/m64p_types.h"
#include "main/util.h"

/* Implements video_input_backend.
 * Needs device field to be set before calling.
 */
static m64p_error opencv_video_open(void* vin, unsigned int width, unsigned int height)
{
    try {
        int dev_num;
        struct opencv_video_backend* b = static_cast<struct opencv_video_backend*>(vin);

        /* allocate memory for cv::VideoCapture */
        cv::VideoCapture* cap = static_cast<cv::VideoCapture*>(std::malloc(sizeof(cv::VideoCapture)));
        if (cap == NULL) {
            DebugMessage(M64MSG_ERROR, "Failed to allocated memory for video device %s", b->device);
            return M64ERR_NO_MEMORY;
        }

        /* placement new to call cv::VideoCapture constructor */
        new(cap)cv::VideoCapture();


        /* open device (we support both device number or path */
        if (string_to_int(b->device, &dev_num)) {
            cap->open(dev_num);
        }
        else {
            cap->open(b->device);
        }

        if (!cap->isOpened()) {
            DebugMessage(M64MSG_ERROR, "Failed to open video device %s", b->device);
            std::free(cap);
            return M64ERR_SYSTEM_FAIL;
        }

        /* TODO: adapt capture resolution to the desired resolution */

        DebugMessage(M64MSG_INFO, "Video successfully opened: %s", b->device);

        b->cap = cap;
        b->width = width;
        b->height = height;

        return M64ERR_SUCCESS;
    }
    /* C++ exception must not cross C-API boundaries */
    catch(...) { return M64ERR_INTERNAL; }
}

static void opencv_video_close(void* vin)
{
    try {
        struct opencv_video_backend* b = static_cast<struct opencv_video_backend*>(vin);

        if (b->cap != NULL) {
            /* explicit call to cv::VideoCapture destructor */
            static_cast<cv::VideoCapture*>(b->cap)->~VideoCapture();

            /* free allocated memory */
            std::free(b->cap);
            b->cap = NULL;
        }

        if (b->device != NULL) {
            std::free(b->device);
            b->device = NULL;
        }

        DebugMessage(M64MSG_INFO, "Video closed");
    }
    /* C++ exception must not cross C-API boundaries */
    catch(...) { return; }
}

static m64p_error opencv_grab_image(void* vin, void* data)
{
    try {
        struct opencv_video_backend* b = static_cast<struct opencv_video_backend*>(vin);
        cv::VideoCapture* cap = static_cast<cv::VideoCapture*>(b->cap);

        /* read next frame */
        cv::Mat frame;
        if (cap == NULL || !cap->read(frame)) {
            DebugMessage(M64MSG_ERROR, "Failed to grab frame !");
            return M64ERR_SYSTEM_FAIL;
        }

        /* resize image to desired resolution */
        cv::Mat output = cv::Mat(b->height, b->width, CV_8UC3, data);
        cv::resize(frame, output, output.size(), 0, 0, cv::INTER_AREA);

        return M64ERR_SUCCESS;
    }
    /* C++ exception must not cross C-API boundaries */
    catch(...) { return M64ERR_INTERNAL; }
}


#if 0
void cv_imshow(const char* name, unsigned int width, unsigned int height, int channels, void* data)
{
    try {
        int type = (channels == 1) ? CV_8UC1 : CV_8UC3;

        cv::Mat frame = cv::Mat(height, width, type, data);
        cv::imshow(name, frame);
        cv::waitKey(1);
    }
    /* C++ exception must not cross C-API boundaries */
    catch(...) { return; }
}
#endif

const struct video_input_backend_interface g_iopencv_video_input_backend =
{
    opencv_video_open,
    opencv_video_close,
    opencv_grab_image
};

}
