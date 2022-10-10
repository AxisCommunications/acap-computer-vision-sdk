/**
* Copyright (C) 2020 Axis Communications AB, Lund, Sweden
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include "precomp.hpp"

#include <map>
#include <string>
#include <algorithm>
#include <system_error>

#include <vdo-map.h>
#include <vdo-error.h>
#include <vdo-buffer.h>
#include <vdo-stream.h>


#include <cmath>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include "cap_axis_internal.hpp"

#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

class ErrnoException : public std::system_error
{
public:
    explicit ErrnoException(const char *what = "") :
        std::system_error(errno, std::generic_category(), what) {}
};

//////////////////////////////////////////////////////////////////////////////
// VDO                                                                      //
//////////////////////////////////////////////////////////////////////////////

using namespace cv;
using std::map;
using std::string;

enum FOURCC : uint32_t
{
    NV12 = CV_FOURCC_MACRO('N','V','1','2'),
    NV21 = CV_FOURCC_MACRO('N','V','2','1'),
    RGB3 = CV_FOURCC_MACRO('R','G','B','3'),
    Y800 = CV_FOURCC_MACRO('Y','8','0','0'),
};

struct FrameSize : public Size_<uint32_t>
{
    uint32_t stride = 0;
};

class VdoCapture : public IVideoCapture
{
private:
    timespec   grabbed_ts = {0};
    FrameSize  current_size;
    FrameSize  capture_size;
    FOURCC     fourcc = FOURCC::NV12;
    size_t     max_buffers = 3;
    AxisInternal* axis_internal = AxisInternal::getInstance();
protected:
    VdoMap*    vdo_prop   = vdo_map_new();
    VdoBuffer* vdo_buffer = nullptr;
    VdoStream* vdo_stream = nullptr;
    map<void*, VdoBuffer*>  buffers;
public:
    virtual double getProperty(int)          const override;
    virtual bool   setProperty(int, double)        override;
    virtual bool   grabFrame()                     override;
    virtual bool   retrieveFrame(int, OutputArray) override;
    virtual bool   isOpened()                const override;
    virtual int    getCaptureDomain()              override
    {
        return CAP_AXIS_VDO;
    }

    virtual ~VdoCapture() override
    {
        if(vdo_buffer)
            vdo_stream_buffer_unref(vdo_stream, &vdo_buffer, NULL);

        g_clear_object(&vdo_prop);
        g_clear_object(&vdo_stream);
    }

    friend Ptr<IVideoCapture> cv::createAxisVDOCapture(int channel);
protected:
    bool create();
    void convert_nv12_to_rgb3();
};

bool VdoCapture::setProperty(int property, double value)
{
    if(!vdo_prop)
        return false;

    uint32_t val32 = uint32_t(std::lround(value));

    switch(property)
    {
    case CAP_PROP_UNIMATRIX_CROP_X: vdo_map_set_uint32(vdo_prop, "crop.x",      val32); return true;
    case CAP_PROP_UNIMATRIX_CROP_Y: vdo_map_set_uint32(vdo_prop, "crop.y",      val32); return true;
    case CAP_PROP_UNIMATRIX_CROP_W: vdo_map_set_uint32(vdo_prop, "crop.width",  val32); return true;
    case CAP_PROP_UNIMATRIX_CROP_H: vdo_map_set_uint32(vdo_prop, "crop.height", val32); return true;
    case CAP_PROP_UNIMATRIX_SIMD_ALIGN: vdo_map_set_uint32(vdo_prop, "simd.align", val32); return true;
    case CAP_PROP_FRAME_WIDTH:  vdo_map_set_uint32(vdo_prop, "width",     val32); return true;
    case CAP_PROP_FRAME_HEIGHT: vdo_map_set_uint32(vdo_prop, "height",    val32); return true;
    case CAP_PROP_CHANNEL:      vdo_map_set_uint32(vdo_prop, "channel",   val32); return true;
    case CAP_PROP_FPS:          vdo_map_set_uint32(vdo_prop, "framerate", val32); return true;
    case CAP_PROP_UNIMATRIX_MAX_BUFFERS:
        max_buffers = val32;
        vdo_map_set_uint32(vdo_prop, "buffer.count", val32);
        return true;
    case CAP_PROP_FOURCC:
        this->fourcc = FOURCC(val32);
        return true;
    // Axis internal properties
    case CAP_PROP_UNIMATRIX_TONEMAPPING:
    case CAP_PROP_UNIMATRIX_TEMPORAL_FILTER:
    case CAP_PROP_UNIMATRIX_EXPOSURE_MODE:
    case CAP_PROP_UNIMATRIX_MAX_GAIN_dB:
    case CAP_PROP_UNIMATRIX_MAX_EXPOSURE_us:
    case CAP_PROP_UNIMATRIX_ROTATION:
        return axis_internal->SetProperty(property, value);
    case CAP_PROP_FORMAT:
    default:
        break;
    }

    return true;
}

double VdoCapture::getProperty(int property) const
{
    if(!vdo_prop)
        return -1;

    switch(property)
    {
    case CAP_PROP_FRAME_WIDTH:
        return double(vdo_map_get_uint32(vdo_prop, "width",  0));
    case CAP_PROP_FRAME_HEIGHT:
        return double(vdo_map_get_uint32(vdo_prop, "height", 0));
    case CAP_PROP_FPS:
        return double(vdo_map_get_uint32(vdo_prop, "framerate", 0));
    case CAP_PROP_UNIMATRIX_CROP_X:
        return double(vdo_map_get_uint32(vdo_prop, "crop.x", 0));
    case CAP_PROP_UNIMATRIX_CROP_Y:
        return double(vdo_map_get_uint32(vdo_prop, "crop.y", 0));
    case CAP_PROP_UNIMATRIX_CROP_W:
        return double(vdo_map_get_uint32(vdo_prop, "crop.width", 0));
    case CAP_PROP_UNIMATRIX_CROP_H:
        return double(vdo_map_get_uint32(vdo_prop, "crop.height", 0));
    case CAP_PROP_UNIMATRIX_SIMD_ALIGN:
        return double(vdo_map_get_uint32(vdo_prop, "simd.align", 1u));
    case CAP_PROP_FOURCC:
        return fourcc;
    case CAP_PROP_FORMAT:
        if(this->fourcc == FOURCC::RGB3)
            return CV_MAKETYPE(CV_8U, 3);
        else
            return CV_MAKETYPE(CV_8U, 1);
    case CAP_PROP_CHANNEL:
        return double(vdo_map_get_uint32(vdo_prop, "channel", 0));
    case CAP_PROP_UNIMATRIX_MAX_BUFFERS:
        return double(vdo_map_get_uint32(vdo_prop, "buffer.count", max_buffers));
    // Axis internal properties
    case CAP_PROP_GAIN:
    case CAP_PROP_EXPOSURE:
    case CAP_PROP_UNIMATRIX_OPTICS_TYPE:
    case CAP_PROP_UNIMATRIX_FNUMBER:
    case CAP_PROP_UNIMATRIX_TONEMAPPING:
    case CAP_PROP_UNIMATRIX_TEMPORAL_FILTER:
    case CAP_PROP_FOCUS:
    case CAP_PROP_ZOOM:
        return axis_internal->GetProperty(property, grabbed_ts);
    }

    if(!vdo_buffer)
        return -1;

    switch(property)
    {
    case CAP_PROP_POS_MSEC:
        return vdo_frame_get_timestamp(vdo_buffer) / 1000.0;
    case CAP_PROP_POS_FRAMES:
        return vdo_frame_get_sequence_nbr(vdo_buffer);
    case CAP_PROP_UNIMATRIX_FD:
        return vdo_buffer_get_fd(vdo_buffer);
    case CAP_PROP_UNIMATRIX_FD_OFFSET:
        return vdo_buffer_get_offset(vdo_buffer);
    case CAP_PROP_UNIMATRIX_FD_CAPACITY:
        return vdo_buffer_get_capacity(vdo_buffer);
    }

    return -1;
}

bool VdoCapture::grabFrame()
{
    g_autoptr(GError) error = NULL;

    if(!vdo_stream)
        create();

    vdo_buffer = nullptr;
    current_size = capture_size;

    if(clock_gettime(CLOCK_MONOTONIC, &grabbed_ts) < 0)
        throw std::runtime_error("Clock Monotonic failed");

    return true;
}

void
VdoCapture::convert_nv12_to_rgb3()
{
    VdoBuffer* nv12_buf = vdo_buffer;
    void* nv12_ptr = vdo_buffer_get_data(nv12_buf);
    if(!nv12_ptr)
        throw std::runtime_error("Invalid NV12 Buffer");

    int buf_fd = memfd_create("opencv.vdo", MFD_CLOEXEC | MFD_ALLOW_SEALING);
    if(buf_fd < 0)
        throw std::runtime_error("Out of memory");

    // Adjust frame height
    current_size.height = 2u * capture_size.height / 3u;

    // Calculate optimal buffer capacity for RGB3
    size_t buf_sz = 3u * current_size.width * current_size.height;
    size_t pagesize = sysconf(_SC_PAGE_SIZE);
    buf_sz = (buf_sz + pagesize) & ~(pagesize - 1);

    if(ftruncate(buf_fd, buf_sz) < 0)
    {
        close(buf_fd);
        throw std::runtime_error("Out of memory");
    }

    g_autoptr(VdoMap) opt = vdo_map_new();
    vdo_map_set_uint32(opt, "buffer.access",   VDO_BUFFER_ACCESS_ANY_RW);
    vdo_map_set_uint32(opt, "buffer.strategy", VDO_BUFFER_STRATEGY_INFINITE);
    VdoBuffer* rgb3_buf = vdo_buffer_new_full(buf_fd, buf_sz, 0, nullptr, opt);
    void* rgb3_ptr = vdo_buffer_get_data(rgb3_buf);
    if(!rgb3_ptr)
        throw std::runtime_error("Invalid RGB3 Buffer");

    // Copy frame meta-data
    uint64_t seqnum = vdo_frame_get_sequence_nbr(nv12_buf);
    vdo_frame_set_sequence_nbr(rgb3_buf, seqnum);
    uint64_t timeus = vdo_frame_get_timestamp(nv12_buf);
    vdo_frame_set_timestamp(rgb3_buf, timeus);

    auto nv12_mat = Mat(capture_size, CV_8UC1, nv12_ptr, capture_size.stride);
    auto rgb3_mat = Mat(current_size, CV_8UC3, rgb3_ptr);

    // Convert into
    cvtColor(nv12_mat, rgb3_mat, cv::COLOR_YUV2RGB_NV12, 3);

    g_autoptr(GError) error = NULL;
    if(!vdo_stream_buffer_unref(vdo_stream, &nv12_buf, &error))
        throw std::runtime_error(error->message);

    // Success - Update global state
    vdo_buffer = rgb3_buf;
}

bool VdoCapture::retrieveFrame(int, OutputArray dst)
{
    g_autoptr(GError) error = NULL;

    // This should be rewritten by saving metadata at the end of the frame.
    // However this lacks VDO support.
    if(auto nbuffers = buffers.size())
    {
        auto& mat = dst.getMatRef();

        // Well-behaved clients will return VdoBuffers facilitating reuse!
        auto it = buffers.find(mat.data);

        // Out of VdoBuffers -> Reuse the oldest one
        if ((it == buffers.cend()) && (nbuffers >= max_buffers))
        {
            using pair = decltype(buffers)::value_type;
            it = std::min_element(buffers.begin(), buffers.end(),
            [] (const pair& lhs, const pair& rhs)
            {
                VdoBuffer* lbuf = lhs.second;
                VdoBuffer* rbuf = rhs.second;
                return vdo_frame_get_sequence_nbr(lbuf) < vdo_frame_get_sequence_nbr(rbuf);
            });
        }

        if(it != buffers.cend())
        {
            VdoBuffer* buffer = it->second;
            buffers.erase(it);

            if(fourcc == FOURCC::RGB3)
                g_object_unref(buffer);
            else
            {
                // Release the buffer and allow the server to reuse it
                if(!vdo_stream_buffer_unref(vdo_stream, &buffer, &error))
                    throw std::runtime_error(error->message);
            }
        }
    }

    uint64_t grabbed_us = uint64_t(grabbed_ts.tv_sec) * 1000000u + (grabbed_ts.tv_nsec / 1000u);

    while(!vdo_buffer)
    {
        VdoBuffer* buffer = vdo_stream_get_buffer(vdo_stream, &error);
        VdoFrame*  frame  = vdo_buffer_get_frame(buffer);

        if(!frame)
            throw std::runtime_error("No Frame");

        uint64_t frame_us = vdo_frame_get_custom_timestamp(frame);
        if((frame_us + 66666u) < grabbed_us)
        {
            // Release the frame & buffer and allow the server to reuse it
            if(!vdo_stream_buffer_unref(vdo_stream, &buffer, &error))
                throw std::runtime_error(error->message);

            std::cout << "Dropping old frames: " << (grabbed_us - frame_us) << std::endl;
            continue;
        }

        // Success!
        vdo_buffer = buffer;
    }

    if(!vdo_buffer_get_data(vdo_buffer))
        throw std::runtime_error("No Data");

    // Convert NV12 to RGB3
    if(fourcc == FOURCC::RGB3)
    {
        if(vdo_frame_get_frame_type(vdo_buffer) == VDO_FRAME_TYPE_YUV)
            convert_nv12_to_rgb3();

        void* data = vdo_buffer_get_data(vdo_buffer);
        buffers[data] = vdo_buffer;
        dst.assign(Mat(current_size, CV_8UC3, data));

        return true;
    }

    void* data = vdo_buffer_get_data(vdo_buffer);
    buffers[data] = vdo_buffer;
    dst.assign(Mat(current_size, CV_8UC1, data, current_size.stride));

    return true;
}

bool VdoCapture::isOpened() const
{
    return true;
}

bool VdoCapture::create()
{
    g_autoptr(GError) error = NULL;

    switch(this->fourcc)
    {
    default: throw std::runtime_error("Unsupported Format");
    case FOURCC::Y800: vdo_map_set_string(vdo_prop, "subformat", "Y800"); break;
    case FOURCC::NV12: vdo_map_set_string(vdo_prop, "subformat", "NV12"); break;
    case FOURCC::NV21: vdo_map_set_string(vdo_prop, "subformat", "NV21"); break;
    case FOURCC::RGB3:
        // VDO lack RGB3 support we have to request NV12 and convert
        vdo_map_set_string(vdo_prop, "subformat", "NV12");

        // Throttle fps when converting large images
        size_t pixels = 1u;
        pixels *= vdo_map_get_uint32(vdo_prop, "width", 0);
        pixels *= vdo_map_get_uint32(vdo_prop, "height", 0);
        pixels  = pixels > (1920u * 1080u);

        if(pixels && vdo_map_get_uint32(vdo_prop, "framerate", 30u) > 15u)
            vdo_map_set_uint32(vdo_prop, "framerate", 15u);
        break;
    }

    vdo_map_set_uint32(vdo_prop, "format", VDO_FORMAT_YUV);
    vdo_map_set_uint32(vdo_prop, "buffer.strategy", VDO_BUFFER_STRATEGY_INFINITE);
    vdo_map_set_uint16(vdo_prop, "timestamp.type", VDO_TIMESTAMP_MONO_SERVER);

    vdo_stream = vdo_stream_new(vdo_prop, nullptr, &error);
    if(!vdo_stream)
        throw std::runtime_error(error->message);

    // 1) 'settings' contains our request + default parameters
    if(g_autoptr(VdoMap) prop = vdo_stream_get_settings(vdo_stream, nullptr))
        vdo_map_merge(vdo_prop, prop);

    // 2) 'info' contains the result from our request such as 'pitch'
    if(g_autoptr(VdoMap) prop = vdo_stream_get_info(vdo_stream, nullptr))
        vdo_map_merge(vdo_prop, prop);

    // Cache CAP_PROP_UNIMATRIX_MAX_BUFFERS
    max_buffers = vdo_map_get_uint32(vdo_prop, "buffer.count", max_buffers);

    // Validate that we got the correct yuv format
    string yuv_format = vdo_map_get_string(vdo_prop, "subformat", NULL, "None");
    if(fourcc == FOURCC::Y800 && yuv_format != "Y800")
        throw std::runtime_error("Unsupported Format: Y800");
    if(fourcc == FOURCC::NV12 && yuv_format != "NV12")
        throw std::runtime_error("Unsupported Format: NV12");
    if(fourcc == FOURCC::NV21 && yuv_format != "NV21")
        throw std::runtime_error("Unsupported Format: NV21");

    capture_size.width  = vdo_map_get_uint32(vdo_prop, "width",  0);
    capture_size.stride = vdo_map_get_uint32(vdo_prop, "pitch", capture_size.width);
    capture_size.height = vdo_map_get_uint32(vdo_prop, "height", 0);

    if(this->fourcc != FOURCC::Y800)
        capture_size.height = 3u * capture_size.height / 2u;

    if(capture_size.stride < capture_size.width)
        throw std::runtime_error("Invalid image stride");

    if(!vdo_stream_start(vdo_stream, &error))
        throw std::runtime_error(error->message);

    return true;
}

// Shared Pointer Factory
Ptr<IVideoCapture> cv::createAxisVDOCapture(int channel)
{
    auto self = makePtr<VdoCapture>();

    self->setProperty(CAP_PROP_CHANNEL, channel);

    return self;
}
