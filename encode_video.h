/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   encode_video.h
 * Author: quangthai
 *
 * Created on August 8, 2016, 10:39 PM
 */

#ifndef ENCODE_VIDEO_H
#define ENCODE_VIDEO_H

/*
 * Copyright (c) 2003 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
/**
 * @file
 * libavformat API example.
 *
 * Output a media file in any supported libavformat format. The default
 * codecs are used.
 * @example muxing.c
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
extern "C" {
#include <libavutil/avassert.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}
#include <cstdlib>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc/types_c.h>
#include <opencv2/highgui.hpp>
#include <opencv2/core/hal/interface.h>
#include <opencv2/imgproc.hpp>
#include <bits/stl_vector.h>
#include <libavutil/pixfmt.h>
#include <opencv2/core/mat.hpp>
#include <libavutil/rational.h>
#include <opencv2/videoio.hpp>

using namespace std;
using namespace cv;

//#define STREAM_DURATION   10.0
//#define STREAM_FRAME_RATE 25 /* 25 images/s */
#define STREAM_PIX_FMT    AV_PIX_FMT_YUV420P /* default pix_fmt */
#define SCALE_FLAGS SWS_BICUBIC
// a wrapper around a single output AVStream

typedef struct OutputStream {
    AVStream *st;
    AVCodecContext *enc;
    /* pts of the next frame that will be generated */
    int64_t next_pts;
    int samples_count;
    AVFrame *frame;
    AVFrame *tmp_frame;
    float t, tincr, tincr2;
    struct SwsContext *sws_ctx;
    struct SwrContext *swr_ctx;
} OutputStream;

static void log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt) {

    //    AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;
    //    printf("pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d\n",
    //            av_ts2str(pkt->pts), av_ts2timestr(pkt->pts, time_base),
    //            av_ts2str(pkt->dts), av_ts2timestr(pkt->dts, time_base),
    //            av_ts2str(pkt->duration), av_ts2timestr(pkt->duration, time_base),
    //            pkt->stream_index);
}

static int write_frame(AVFormatContext *fmt_ctx, const AVRational *time_base, AVStream *st, AVPacket *pkt) {
    /* rescale output packet timestamp values from codec to stream timebase */
    av_packet_rescale_ts(pkt, *time_base, st->time_base);
    pkt->stream_index = st->index;
    /* Write the compressed frame to the media file. */
    log_packet(fmt_ctx, pkt);
    return av_interleaved_write_frame(fmt_ctx, pkt);
}

/* Add an output stream. */
static void add_stream(OutputStream *ost, AVFormatContext *oc,
        AVCodec **codec,
        enum AVCodecID codec_id,
        float quality, int fps, int width, int height) {
    AVCodecContext *c;
    int i;
    /* find the encoder */
    *codec = avcodec_find_encoder(codec_id);
    if (!(*codec)) {
        fprintf(stderr, "Could not find encoder for '%s'\n",
                avcodec_get_name(codec_id));
        exit(1);
    }
    ost->st = avformat_new_stream(oc, NULL);
    if (!ost->st) {
        fprintf(stderr, "Could not allocate stream\n");
        exit(1);
    }
    ost->st->id = oc->nb_streams - 1;
    c = avcodec_alloc_context3(*codec);
    if (!c) {
        fprintf(stderr, "Could not alloc an encoding context\n");
        exit(1);
    }
    ost->enc = c;
    switch ((*codec)->type) {
        case AVMEDIA_TYPE_AUDIO:
            c->sample_fmt = (*codec)->sample_fmts ?
                    ((*codec)->sample_fmts[0]) : AV_SAMPLE_FMT_FLTP;
            c->bit_rate = 64000;
            c->sample_rate = 44100;
            if ((*codec)->supported_samplerates) {
                c->sample_rate = (*codec)->supported_samplerates[0];
                for (i = 0; (*codec)->supported_samplerates[i]; i++) {
                    if ((*codec)->supported_samplerates[i] == 44100)
                        c->sample_rate = 44100;
                }
            }
            c->channels = av_get_channel_layout_nb_channels(c->channel_layout);
            c->channel_layout = AV_CH_LAYOUT_STEREO;
            if ((*codec)->channel_layouts) {
                c->channel_layout = (*codec)->channel_layouts[0];
                for (i = 0; (*codec)->channel_layouts[i]; i++) {
                    if ((*codec)->channel_layouts[i] == AV_CH_LAYOUT_STEREO)
                        c->channel_layout = AV_CH_LAYOUT_STEREO;
                }
            }
            c->channels = av_get_channel_layout_nb_channels(c->channel_layout);
            ost->st->time_base = (AVRational){1, c->sample_rate};
            break;
        case AVMEDIA_TYPE_VIDEO:
            c->codec_id = codec_id;
            c->bit_rate = static_cast<int64_t> (floor(400000.0 * quality / 100));
            /* Resolution must be a multiple of two. */
            c->width = width;
            c->height = height;
            /* timebase: This is the fundamental unit of time (in seconds) in terms
             * of which frame timestamps are represented. For fixed-fps content,
             * timebase should be 1/framerate and timestamp increments should be
             * identical to 1. */
            ost->st->time_base = (AVRational){1, fps};
            c->time_base = ost->st->time_base;
            c->gop_size = 12; /* emit one intra frame every twelve frames at most */
            c->pix_fmt = STREAM_PIX_FMT;
            if (c->codec_id == AV_CODEC_ID_MPEG2VIDEO) {
                /* just for testing, we also add B-frames */
                c->max_b_frames = 2;
            }
            if (c->codec_id == AV_CODEC_ID_MPEG1VIDEO) {
                /* Needed to avoid using macroblocks in which some coeffs overflow.
                 * This does not happen with normal video, it just happens here as
                 * the motion of the chroma plane does not match the luma plane. */
                c->mb_decision = 2;
            }
            break;
        default:
            break;
    }
    /* Some formats want stream headers to be separate. */
    if (oc->oformat->flags & AVFMT_GLOBALHEADER)
        c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
}
/**************************************************************/

/* video output */
static AVFrame *alloc_picture(enum AVPixelFormat pix_fmt, int width, int height) {
    AVFrame *picture;
    int ret;
    picture = av_frame_alloc();
    if (!picture)
        return NULL;
    picture->format = pix_fmt;
    picture->width = width;
    picture->height = height;
    /* allocate the buffers for the frame data */
    ret = av_frame_get_buffer(picture, 32);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate frame data.\n");
        exit(1);
    }
    return picture;
}

static void open_video(AVFormatContext *oc, AVCodec *codec, OutputStream *ost, AVDictionary *opt_arg) {
    int ret;
    AVCodecContext *c = ost->enc;
    AVDictionary *opt = NULL;
    av_dict_copy(&opt, opt_arg, 0);
    /* open the codec */
    ret = avcodec_open2(c, codec, &opt);
    av_dict_free(&opt);
    if (ret < 0) {
        //        fprintf(stderr, "Could not open video codec: %s\n", av_err2str(ret));
        exit(1);
    }
    /* allocate and init a re-usable frame */
    ost->frame = alloc_picture(c->pix_fmt, c->width, c->height);
    if (!ost->frame) {
        fprintf(stderr, "Could not allocate video frame\n");
        exit(1);
    }
    /* If the output format is not YUV420P, then a temporary YUV420P
     * picture is needed too. It is then converted to the required
     * output format. */
    ost->tmp_frame = NULL;
    if (c->pix_fmt != AV_PIX_FMT_YUV420P) {
        ost->tmp_frame = alloc_picture(AV_PIX_FMT_YUV420P, c->width, c->height);
        if (!ost->tmp_frame) {
            fprintf(stderr, "Could not allocate temporary picture\n");
            exit(1);
        }
    }
    /* copy the stream parameters to the muxer */
    ret = avcodec_parameters_from_context(ost->st->codecpar, c);
    if (ret < 0) {
        fprintf(stderr, "Could not copy the stream parameters\n");
        exit(1);
    }
}

void cvtFrame2AVFrameYUV444(Mat& frame, AVFrame **avframe) {
    Mat frameYUV;
    frame.convertTo(frameYUV, CV_32FC3);
    cvtColor(frameYUV, frameYUV, CV_BGR2YCrCb);
    for (int y = 0; y < frameYUV.rows; y++) {
        for (int x = 0; x < frameYUV.cols; x++) {
            Vec3f& pixel = frameYUV.at<Vec3f>(y, x);
            (*avframe)->data[0][y * (*avframe)->linesize[0] + x] = static_cast<uint8_t> (pixel[0]);
            (*avframe)->data[1][y * (*avframe)->linesize[1] + x] = static_cast<uint8_t> (ceil(pixel[1]*255));
            (*avframe)->data[2][y * (*avframe)->linesize[2] + x] = static_cast<uint8_t> (ceil(pixel[2]*255));
        }
    }
}

void cvtFrame2AVFrameYUV420(Mat& frame, AVFrame **avframe) {
    Mat frameYUV;
    cvtColor(frame, frameYUV, CV_BGR2YUV_I420);
    for (int y = 0; y < frame.rows; y++) {
        for (int x = 0; x < frame.cols; x++) {
            uchar value = frameYUV.at<uchar>(y, x);
            (*avframe)->data[0][y * (*avframe)->linesize[0] + x] = static_cast<uint8_t> (value);
        }
    }
    Mat UVImg = frameYUV(Rect(0, frame.rows, frame.cols, frameYUV.rows - frame.rows));
    int yU = 0;
    for (int y = 0; y < UVImg.rows / 2; y++) {
        for (int x = 0; x < UVImg.cols / 2; x++) {
            uchar value = UVImg.at<uchar>(y, x);
            (*avframe)->data[1][yU * (*avframe)->linesize[1] + x] = static_cast<uint8_t> (value);
        }
        yU++;
        for (int x = 0; x < UVImg.cols / 2; x++) {
            uchar value = UVImg.at<uchar>(y, x + UVImg.cols / 2);
            (*avframe)->data[1][yU * (*avframe)->linesize[1] + x] = static_cast<uint8_t> (value);
        }
        yU++;
    }
    int yV = 0;
    for (int y = UVImg.rows / 2; y < UVImg.rows; y++) {
        for (int x = 0; x < UVImg.cols / 2; x++) {
            uchar value = UVImg.at<uchar>(y, x);
            (*avframe)->data[2][yV * (*avframe)->linesize[2] + x] = static_cast<uint8_t> (value);
        }
        yV++;
        for (int x = 0; x < UVImg.cols / 2; x++) {
            uchar value = UVImg.at<uchar>(y, x + UVImg.cols / 2);
            (*avframe)->data[2][yV * (*avframe)->linesize[2] + x] = static_cast<uint8_t> (value);
        }
        yV++;
    }
}

/* Prepare a dummy image. */
static void fill_yuv_image(AVFrame *pict, vector<Mat>& frames, int frame_index,
        int width, int height) {
    int x, y, i, ret;
    /* when we pass a frame to the encoder, it may keep a reference to it
     * internally;
     * make sure we do not overwrite it here
     */
    ret = av_frame_make_writable(pict);
    if (ret < 0)
        exit(1);
    Mat& frame = frames[frame_index];
    cvtFrame2AVFrameYUV420(frame, &pict);

    //    i = frame_index;
    //    /* Y */
    //    for (y = 0; y < height; y++)
    //        for (x = 0; x < width; x++)
    //            pict->data[0][y * pict->linesize[0] + x] = x + y + i * 3;
    //    /* Cb and Cr */
    //    for (y = 0; y < height / 2; y++) {
    //        for (x = 0; x < width / 2; x++) {
    //            pict->data[1][y * pict->linesize[1] + x] = 128 + y + i * 2;
    //            pict->data[2][y * pict->linesize[2] + x] = 64 + x + i * 5;
    //        }
    //    }
}

static AVFrame *get_video_frame(OutputStream *ost, vector<Mat>& frames) {
    AVCodecContext *c = ost->enc;
    /* check if we want to generate more frames */
    //    if (av_compare_ts(ost->next_pts, c->time_base,
    //            STREAM_DURATION, (AVRational) {
    //            1, 1 }) >= 0)
    //    return NULL;
    if (ost->next_pts >= frames.size()) {
        return NULL;
    }
    if (c->pix_fmt != AV_PIX_FMT_YUV420P) {
        /* as we only generate a YUV420P picture, we must convert it
         * to the codec pixel format if needed */
        if (!ost->sws_ctx) {
            ost->sws_ctx = sws_getContext(c->width, c->height,
                    AV_PIX_FMT_YUV420P,
                    c->width, c->height,
                    c->pix_fmt,
                    SCALE_FLAGS, NULL, NULL, NULL);
            if (!ost->sws_ctx) {
                fprintf(stderr,
                        "Could not initialize the conversion context\n");
                exit(1);
            }
        }
        fill_yuv_image(ost->tmp_frame, frames, ost->next_pts, c->width, c->height);
        sws_scale(ost->sws_ctx,
                (const uint8_t * const *) ost->tmp_frame->data, ost->tmp_frame->linesize,
                0, c->height, ost->frame->data, ost->frame->linesize);
    } else {
        fill_yuv_image(ost->frame, frames, ost->next_pts, c->width, c->height);
    }
    ost->frame->pts = ost->next_pts++;
    return ost->frame;
}

/*
 * encode one video frame and send it to the muxer
 * return 1 when encoding is finished, 0 otherwise
 */
static int write_video_frame(AVFormatContext *oc, OutputStream *ost, vector<Mat>& frames) {
    int ret;
    AVCodecContext *c;
    AVFrame *frame;
    int got_packet = 0;
    AVPacket pkt = {0};
    c = ost->enc;
    frame = get_video_frame(ost, frames);
    av_init_packet(&pkt);
    /* encode the image */
    ret = avcodec_encode_video2(c, &pkt, frame, &got_packet);
    if (ret < 0) {
        //        fprintf(stderr, "Error encoding video frame: %s\n", av_err2str(ret));
        exit(1);
    }
    if (got_packet) {
        ret = write_frame(oc, &c->time_base, ost->st, &pkt);
    } else {
        ret = 0;
    }
    if (ret < 0) {
        //        fprintf(stderr, "Error while writing video frame: %s\n", av_err2str(ret));
        exit(1);
    }
    return (frame || got_packet) ? 0 : 1;
}

static void close_stream(AVFormatContext *oc, OutputStream *ost) {
    avcodec_free_context(&ost->enc);
    av_frame_free(&ost->frame);
    av_frame_free(&ost->tmp_frame);
    sws_freeContext(ost->sws_ctx);
    swr_free(&ost->swr_ctx);
}
/**************************************************************/

/* media file output */
int encode_video(const char *filename, vector<Mat>& frames, float quality, int fps, int refFrameIndex, AVCodecID codec_id) {
    OutputStream video_st = {0}, audio_st = {0};
    AVOutputFormat *fmt;
    AVFormatContext *oc;
    AVCodec *video_codec;
    int ret;
    int have_video = 0, have_audio = 0;
    int encode_video = 0, encode_audio = 0;
    AVDictionary *opt = NULL;
    int i;
    /* Initialize libavcodec, and register all codecs and formats. */
    av_register_all();
    //    if (argc < 2) {
    //        printf("usage: %s output_file\n"
    //                "API example program to output a media file with libavformat.\n"
    //                "This program generates a synthetic audio and video stream, encodes and\n"
    //                "muxes them into a file named output_file.\n"
    //                "The output format is automatically guessed according to the file extension.\n"
    //                "Raw images can also be output by using '%%d' in the filename.\n"
    //                "\n", argv[0]);
    //        return 1;
    //    }
    //    filename = "/home/quangthai/Desktop/Workspace/test.mp4";
    //    for (i = 2; i + 1 < argc; i += 2) {
    //        if (!strcmp(argv[i], "-flags") || !strcmp(argv[i], "-fflags"))
    //            av_dict_set(&opt, argv[i] + 1, argv[i + 1], 0);
    //    }
    /* allocate the output media context */
    avformat_alloc_output_context2(&oc, NULL, NULL, filename);
    if (!oc) {
        printf("Could not deduce output format from file extension: using MPEG.\n");
        avformat_alloc_output_context2(&oc, NULL, "mpeg", filename);
    }
    if (!oc)
        return 1;
    fmt = oc->oformat;
    fmt->video_codec = codec_id;
    /* Add the audio and video streams using the default format codecs
     * and initialize the codecs. */
    if (fmt->video_codec != AV_CODEC_ID_NONE) {
        add_stream(&video_st, oc, &video_codec, fmt->video_codec,
                quality, fps, frames[0].cols, frames[0].rows);
        have_video = 1;
        encode_video = 1;
    }
    //    if (fmt->audio_codec != AV_CODEC_ID_NONE) {
    //        add_stream(&audio_st, oc, &audio_codec, fmt->audio_codec);
    //        have_audio = 1;
    //        encode_audio = 1;
    //    }
    /* Now that all the parameters are set, we can open the audio and
     * video codecs and allocate the necessary encode buffers. */
    if (have_video)
        open_video(oc, video_codec, &video_st, opt);
    //    if (have_audio)
    //        open_audio(oc, audio_codec, &audio_st, opt);
    av_dump_format(oc, 0, filename, 1);
    /* open the output file, if needed */
    if (!(fmt->flags & AVFMT_NOFILE)) {
        ret = avio_open(&oc->pb, filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            //            fprintf(stderr, "Could not open '%s': %s\n", filename,
            //                    av_err2str(ret));
            return 1;
        }
    }
    /* Write the stream header, if any. */
    string sTitle;
    sTitle.push_back(refFrameIndex);
    av_dict_set(&opt, "title", sTitle.c_str(), 0);
    oc->metadata = opt;
    ret = avformat_write_header(oc, NULL);
    if (ret < 0) {
        //        fprintf(stderr, "Error occurred when opening output file: %s\n",
        //                av_err2str(ret));
        return 1;
    }
    while (encode_video || encode_audio) {
        /* select the stream to encode */
        if (encode_video &&
                (!encode_audio || av_compare_ts(video_st.next_pts, video_st.enc->time_base,
                audio_st.next_pts, audio_st.enc->time_base) <= 0)) {
            encode_video = !write_video_frame(oc, &video_st, frames);
        } else {
            //            encode_audio = !write_audio_frame(oc, &audio_st);
        }
    }
    /* Write the trailer, if any. The trailer must be written before you
     * close the CodecContexts open when you wrote the header; otherwise
     * av_write_trailer() may try to use memory that was freed on
     * av_codec_close(). */
    av_write_trailer(oc);
    /* Close each codec. */
    if (have_video)
        close_stream(oc, &video_st);
    if (have_audio)
        close_stream(oc, &audio_st);
    if (!(fmt->flags & AVFMT_NOFILE))
        /* Close the output file. */
        avio_closep(&oc->pb);
    /* free the stream */
    avformat_free_context(oc);
    return 0;
}

void encodeVideo(const char *filename, vector<Mat>& frames, float quality, int fps, AVCodecID codec_id) {
    int w = frames[0].cols;
    int h = frames[0].rows;
    //    cout << frames.size() << endl;

    AVCodec *codec;
    AVCodecContext *c = NULL;
    int i, ret, x, y, got_output;
    FILE *f;
    AVFrame *frame;
    AVPacket pkt;
    uint8_t endcode[] = {0, 0, 1, 0xb7};

    //    printf("Encode video file %s\n", filename);

    /* find the video encoder */
    codec = avcodec_find_encoder(codec_id);
    if (!codec) {
        fprintf(stderr, "Codec not found\n");
        exit(1);
    }

    c = avcodec_alloc_context3(codec);
    if (!c) {
        fprintf(stderr, "Could not allocate video codec context\n");
        exit(1);
    }

    /* put sample parameters */
    //        c->bit_rate = 400000;
    c->bit_rate = static_cast<int64_t> (floor(quality * 400000 / 100));
    //    c->bit_rate = static_cast<int64_t> (32 * 1024);
    c->global_quality = 1;
    /* resolution must be a multiple of two */
    c->width = w;
    c->height = h;
    /* frames per second */
    c->time_base = (AVRational){1, fps};
    /* emit one intra frame every ten frames
     * check frame pict_type before passing frame
     * to encoder, if frame->pict_type is AV_PICTURE_TYPE_I
     * then gop_size is ignored and the output of encoder
     * will always be I frame irrespective to gop_size
     */
    c->gop_size = 10;
    c->max_b_frames = 1;
    c->pix_fmt = AV_PIX_FMT_YUV420P;

    if (codec_id == AV_CODEC_ID_H264)
        av_opt_set(c->priv_data, "preset", "slow", 0);

    /* open it */
    if (avcodec_open2(c, codec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        exit(1);
    }

    f = fopen(filename, "wb");
    if (!f) {
        fprintf(stderr, "Could not open %s\n", filename);
        exit(1);
    }

    frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Could not allocate video frame\n");
        exit(1);
    }
    frame->format = c->pix_fmt;
    frame->width = c->width;
    frame->height = c->height;

    /* the image can be allocated by any means and av_image_alloc() is
     * just the most convenient way if av_malloc() is to be used */
    ret = av_image_alloc(frame->data, frame->linesize, c->width, c->height,
            c->pix_fmt, 32);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate raw picture buffer\n");
        exit(1);
    }

    //        AVOutputFormat *fmt;
    //        fmt = av_guess_format(NULL, filename, NULL);
    //        if (!fmt){
    //            printf("Could not deduce output format from file extension: using MPEG.\n");
    //            fmt = av_guess_format("mpeg", NULL, NULL);
    //        }
    //        if (!fmt){
    //            fprintf(stderr, "Could not find suitable output format\n");
    //            exit(1);
    //        }
    //        fmt->video_codec = codec_id;
    //    
    //        AVFormatContext *oc;
    //        oc = avformat_alloc_context();
    //        if (!oc){
    //            fprintf(stderr, "Memory error\n");
    //            exit(1);
    //        }
    //        oc->oformat = fmt;
    //        av_dump_format(oc, 0, filename, 1);
    //        
    //        AVDictionary *opts = NULL;
    //        av_dict_set(&opts, "title", "Test Title", 0);
    //        oc->metadata = opts;
    //        
    //        ret = avformat_write_header(oc, NULL);

    /* encode 1 second of video */
    //    for (i = 0; i < 25; i++) {
    for (int i = 0; i < frames.size(); i++) {
        av_init_packet(&pkt);
        pkt.data = NULL; // packet data will be allocated by the encoder
        pkt.size = 0;

        fflush(stdout);

        if (c->pix_fmt == AV_PIX_FMT_YUV420P) {
            cvtFrame2AVFrameYUV420(frames[i], &frame);
        } else if (c->pix_fmt == AV_PIX_FMT_YUV444P) {
            cvtFrame2AVFrameYUV444(frames[i], &frame);
        }

        frame->pts = i;

        /* encode the image */
        ret = avcodec_encode_video2(c, &pkt, frame, &got_output);
        if (ret < 0) {
            fprintf(stderr, "Error encoding frame\n");
            exit(1);
        }

        if (got_output) {
            //            printf("Write frame %3d (size=%5d)\n", i, pkt.size);5;mnpxz
            fwrite(pkt.data, 1, pkt.size, f);
            av_packet_unref(&pkt);
        }
    }

    /* add sequence end code to have a real MPEG file */
    fwrite(endcode, 1, sizeof (endcode), f);
    fclose(f);

    avcodec_close(c);
    av_free(c);
    //    av_freep(&frame->data[0]);
    av_frame_free(&frame);
    printf("\n");
}

void read_video_description(const char *filename, vector<array<string, 2 >> &metadata) {
    AVFormatContext *fmt_ctx = NULL;
    AVDictionaryEntry *tag = NULL;
    int ret;
    av_register_all();
    if ((ret = avformat_open_input(&fmt_ctx, filename, NULL, NULL))) {
        printf("Error: can't read video description\n");
    } else {
        while ((tag = av_dict_get(fmt_ctx->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
            //            printf("%s = %s\n", tag->key, tag->value);
            array<string, 2> keyValue = {string(tag->key), string(tag->value)};
            metadata.push_back(keyValue);
        }
        avformat_close_input(&fmt_ctx);
    }
}

#endif /* ENCODE_VIDEO_H */

