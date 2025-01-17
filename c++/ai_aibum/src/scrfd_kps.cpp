#if defined(USE_NCNN_SIMPLEOCV)
#include "simpleocv.h"
#else
#include <opencv2/imgproc/imgproc.hpp>
#endif
#include <vector>
#include "face_de/include/scrfd_kps.h"

int Detect::detect_init(const std::string& param_model_path, const std::string& bin_model_path)
{
    scrfd.clear();
#if NCNN_VULKAN
    scrfd.opt.num_threads = 2;
    scrfd.opt.use_fp16_arithmetic = true; // 启用 FP16 计算
    scrfd.opt.use_vulkan_compute = true; // 启用 Vulkan 加速
    scrfd.opt.use_int8_inference = true; // 启用 INT8 推理
#endif
    if (scrfd.load_param(param_model_path.c_str()) != 0)
        return false;
    if (scrfd.load_model(bin_model_path.c_str()) != 0)
        return false;
    return true;
}

Detect::Detect(int target_size)
{
    this->target_size = target_size;
}

Detect::Detect(int target_size,
               float prob_threshold,
               float nms_threshold)
{
    this->target_size = target_size;
    this->prob_threshold = prob_threshold;
    this->nms_threshold = nms_threshold;
}

Detect::~Detect()
{
    this->scrfd.clear();
}

#include "iostream"

int Detect::detect_scrfd(const cv::Mat& bgr,
                         std::vector<FaceObject>& faceobjects)
{
    auto start = std::chrono::high_resolution_clock::now();
    int width = bgr.cols;
    int height = bgr.rows;
    int w = width;
    int h = height;
    float scale = 1.f;
    if (w > h)
    {
        scale = (float)target_size / w;
        w = target_size;
        h = static_cast<int>(height * scale);
    }
    else
    {
        scale = (float)target_size / h;
        h = target_size;
        w = static_cast<int>(width * scale);
    }
    // 缩放图像
    ncnn::Mat in = ncnn::Mat::from_pixels_resize(bgr.data, ncnn::Mat::PIXEL_BGR2RGB, width, height, w, h);
    int top = 0;
    int bottom = std::max(0, target_size - h);
    int left = 0;
    int right = std::max(0, target_size - w);
    ncnn::Mat in_pad;
    ncnn::copy_make_border(in, in_pad, top, bottom, left, right, ncnn::BORDER_CONSTANT, 0.f);
    const float mean_vals[3] = {127.5f, 127.5f, 127.5f};
    const float norm_vals[3] = {1 / 128.f, 1 / 128.f, 1 / 128.f};
    in_pad.substract_mean_normalize(mean_vals, norm_vals);
    ncnn::Extractor ex = scrfd.create_extractor();
    ex.input("input.1", in_pad);
    std::vector<FaceObject> faceproposals;
    // stride 32
    {
        ncnn::Mat score_blob, bbox_blob, landmark_blob;
        ex.extract("score_32", score_blob);
        ex.extract("bbox_32", bbox_blob);
        ex.extract("kps_32", landmark_blob);
        const int base_size = 256;
        const int feat_stride = 32;
        ncnn::Mat ratios(1);
        ratios[0] = 1.f;
        ncnn::Mat scales(2);
        scales[0] = 1.f;
        scales[1] = 2.f;
        ncnn::Mat anchors = generate_anchors(base_size, ratios, scales);
        std::vector<FaceObject> faceobjects32;
        generate_proposals(anchors, feat_stride, score_blob, bbox_blob, landmark_blob, prob_threshold, faceobjects32);
        faceproposals.insert(faceproposals.end(), faceobjects32.begin(), faceobjects32.end());
    }
    // stride 16
    {
        ncnn::Mat score_blob, bbox_blob, landmark_blob;
        ex.extract("score_16", score_blob);
        ex.extract("bbox_16", bbox_blob);
        ex.extract("kps_16", landmark_blob);
        const int base_size = 64;
        const int feat_stride = 16;
        ncnn::Mat ratios(1);
        ratios[0] = 1.f;
        ncnn::Mat scales(2);
        scales[0] = 1.f;
        scales[1] = 2.f;
        ncnn::Mat anchors = generate_anchors(base_size, ratios, scales);
        std::vector<FaceObject> faceobjects16;
        generate_proposals(anchors, feat_stride, score_blob, bbox_blob, landmark_blob, prob_threshold, faceobjects16);
        faceproposals.insert(faceproposals.end(), faceobjects16.begin(), faceobjects16.end());
    }
    // stride 8
    {
        ncnn::Mat score_blob, bbox_blob, landmark_blob;
        ex.extract("score_8", score_blob);
        ex.extract("bbox_8", bbox_blob);
        ex.extract("kps_8", landmark_blob);
        const int base_size = 16;
        const int feat_stride = 8;
        ncnn::Mat ratios(1);
        ratios[0] = 1.f;
        ncnn::Mat scales(2);
        scales[0] = 1.f;
        scales[1] = 2.f;
        ncnn::Mat anchors = generate_anchors(base_size, ratios, scales);
        std::vector<FaceObject> faceobjects8;
        generate_proposals(anchors, feat_stride, score_blob, bbox_blob, landmark_blob, prob_threshold, faceobjects8);
        faceproposals.insert(faceproposals.end(), faceobjects8.begin(), faceobjects8.end());
    }
    // sort all proposals by score from highest to lowest
    qsort_descent_inplace(faceproposals);
    // apply nms with nms_threshold
    std::vector<int> picked;
    nms_sorted_bboxes(faceproposals, picked, nms_threshold);

    int face_count = picked.size();

    faceobjects.resize(face_count);
    std::cout << "face_count: " << face_count << std::endl;
    for (int i = 0; i < face_count; i++)
    {
        faceobjects[i] = faceproposals[picked[i]];

        // adjust offset to original unpadded
        float x0 = (faceobjects[i].rect.x) / scale;
        float y0 = (faceobjects[i].rect.y) / scale;
        float x1 = (faceobjects[i].rect.x + faceobjects[i].rect.width) / scale;
        float y1 = (faceobjects[i].rect.y + faceobjects[i].rect.height) / scale;

        x0 = std::max(std::min(x0, (float)width - 1), 0.f);
        y0 = std::max(std::min(y0, (float)height - 1), 0.f);
        x1 = std::max(std::min(x1, (float)width - 1), 0.f);
        y1 = std::max(std::min(y1, (float)height - 1), 0.f);

        faceobjects[i].rect.x = x0;
        faceobjects[i].rect.y = y0;
        faceobjects[i].rect.width = x1 - x0;
        faceobjects[i].rect.height = y1 - y0;

        faceobjects[i].landmark[0].x /= scale;
        faceobjects[i].landmark[1].x /= scale;
        faceobjects[i].landmark[2].x /= scale;
        faceobjects[i].landmark[3].x /= scale;
        faceobjects[i].landmark[4].x /= scale;

        faceobjects[i].landmark[0].y /= scale;
        faceobjects[i].landmark[1].y /= scale;
        faceobjects[i].landmark[2].y /= scale;
        faceobjects[i].landmark[3].y /= scale;
        faceobjects[i].landmark[4].y /= scale;
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsedSeconds = end - start;
    // auto elapsedMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(elapsedSeconds);
    // std::cout << "detect_scrfd: " << elapsedSeconds.count() << " seconds," << std::endl;
    return 0;
}

float Detect::intersection_area(const FaceObject& a, const FaceObject& b)
{
    cv::Rect_<float> inter = a.rect & b.rect;
    return inter.area();
}

void Detect::qsort_descent_inplace(std::vector<FaceObject>& faceobjects, int left, int right)
{
    int i = left;
    int j = right;
    float p = faceobjects[(left + right) / 2].prob;

    while (i <= j)
    {
        while (faceobjects[i].prob > p)
            i++;

        while (faceobjects[j].prob < p)
            j--;

        if (i <= j)
        {
            // swap
            std::swap(faceobjects[i], faceobjects[j]);

            i++;
            j--;
        }
    }

#pragma omp parallel sections
    {
#pragma omp section
        {
            if (left < j) qsort_descent_inplace(faceobjects, left, j);
        }
#pragma omp section
        {
            if (i < right) qsort_descent_inplace(faceobjects, i, right);
        }
    }
}

void Detect::qsort_descent_inplace(std::vector<FaceObject>& faceobjects)
{
    if (faceobjects.empty())
        return;

    qsort_descent_inplace(faceobjects, 0, faceobjects.size() - 1);
}

void Detect::nms_sorted_bboxes(const std::vector<FaceObject>& faceobjects, std::vector<int>& picked,
                               float nms_threshold)
{
    picked.clear();
    const int n = faceobjects.size();
    std::vector<float> areas(n);
    for (int i = 0; i < n; i++)
    {
        areas[i] = faceobjects[i].rect.area();
    }

    for (int i = 0; i < n; i++)
    {
        const FaceObject& a = faceobjects[i];

        int keep = 1;
        for (int j = 0; j < (int)picked.size(); j++)
        {
            const FaceObject& b = faceobjects[picked[j]];

            // intersection over union
            float inter_area = intersection_area(a, b);
            float union_area = areas[i] + areas[picked[j]] - inter_area;
            //             float IoU = inter_area / union_area
            if (inter_area / union_area > nms_threshold)
                keep = 0;
        }

        if (keep)
            picked.push_back(i);
    }
}

ncnn::Mat Detect::generate_anchors(int base_size, const ncnn::Mat& ratios, const ncnn::Mat& scales)
{
    int num_ratio = ratios.w;
    int num_scale = scales.w;

    ncnn::Mat anchors;
    anchors.create(4, num_ratio * num_scale);

    const float cx = 0;
    const float cy = 0;

    for (int i = 0; i < num_ratio; i++)
    {
        float ar = ratios[i];

        int r_w = round(base_size / sqrt(ar));
        int r_h = round(r_w * ar); //round(base_size * sqrt(ar));

        for (int j = 0; j < num_scale; j++)
        {
            float scale = scales[j];

            float rs_w = r_w * scale;
            float rs_h = r_h * scale;

            float* anchor = anchors.row(i * num_scale + j);

            anchor[0] = cx - rs_w * 0.5f;
            anchor[1] = cy - rs_h * 0.5f;
            anchor[2] = cx + rs_w * 0.5f;
            anchor[3] = cy + rs_h * 0.5f;
        }
    }

    return anchors;
}

void Detect::generate_proposals(const ncnn::Mat& anchors, int feat_stride, const ncnn::Mat& score_blob,
                                const ncnn::Mat& bbox_blob, const ncnn::Mat& landmark_blob, float prob_threshold,
                                std::vector<FaceObject>& faceobjects)
{
    int w = score_blob.w;
    int h = score_blob.h;

    // generate face proposal from bbox deltas and shifted anchors
    const int num_anchors = anchors.h;

    for (int q = 0; q < num_anchors; q++)
    {
        const float* anchor = anchors.row(q);

        const ncnn::Mat score = score_blob.channel(q);
        const ncnn::Mat bbox = bbox_blob.channel_range(q * 4, 4);
        const ncnn::Mat landmark = landmark_blob.channel_range(q * 10, 10);

        // shifted anchor
        float anchor_y = anchor[1];

        float anchor_w = anchor[2] - anchor[0];
        float anchor_h = anchor[3] - anchor[1];

        for (int i = 0; i < h; i++)
        {
            float anchor_x = anchor[0];

            for (int j = 0; j < w; j++)
            {
                int index = i * w + j;

                float prob = score[index];

                if (prob >= prob_threshold)
                {
                    // apply center size
                    float dx = bbox.channel(0)[index] * feat_stride;
                    float dy = bbox.channel(1)[index] * feat_stride;
                    float dw = bbox.channel(2)[index] * feat_stride;
                    float dh = bbox.channel(3)[index] * feat_stride;

                    float cx = anchor_x + anchor_w * 0.5f;
                    float cy = anchor_y + anchor_h * 0.5f;

                    float x0 = cx - dx;
                    float y0 = cy - dy;
                    float x1 = cx + dw;
                    float y1 = cy + dh;

                    FaceObject obj;
                    obj.rect.x = x0;
                    obj.rect.y = y0;
                    obj.rect.width = x1 - x0 + 1;
                    obj.rect.height = y1 - y0 + 1;

                    //printf("%4.2f, %4.2f, %4.2f, %4.2f\n", x0, y0, x1 - x0 + 1, y1 - y0 + 1);

                    obj.landmark[0].x = cx + landmark.channel(0)[index] * feat_stride;
                    obj.landmark[0].y = cy + landmark.channel(1)[index] * feat_stride;
                    obj.landmark[1].x = cx + landmark.channel(2)[index] * feat_stride;
                    obj.landmark[1].y = cy + landmark.channel(3)[index] * feat_stride;
                    obj.landmark[2].x = cx + landmark.channel(4)[index] * feat_stride;
                    obj.landmark[2].y = cy + landmark.channel(5)[index] * feat_stride;
                    obj.landmark[3].x = cx + landmark.channel(6)[index] * feat_stride;
                    obj.landmark[3].y = cy + landmark.channel(7)[index] * feat_stride;
                    obj.landmark[4].x = cx + landmark.channel(8)[index] * feat_stride;
                    obj.landmark[4].y = cy + landmark.channel(9)[index] * feat_stride;

                    obj.prob = prob;

                    faceobjects.push_back(obj);
                }

                anchor_x += feat_stride;
            }

            anchor_y += feat_stride;
        }
    }
}

