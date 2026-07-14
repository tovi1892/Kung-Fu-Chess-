#include "img.hpp"
#include <iostream>
#include <stdexcept>

Img::Img() {
    // Constructor - img is automatically initialized as empty
}

Img& Img::read(const std::string& path,
               const std::pair<int, int>& size,
               bool keep_aspect,
               int interpolation) {
    img = cv::imread(path, cv::IMREAD_UNCHANGED);
    if (img.empty()) {
        throw std::runtime_error("Cannot load image: " + path);
    }

    if (size.first != 0 && size.second != 0) {  // Check if size is not empty
        int target_w = size.first;
        int target_h = size.second;
        int h = img.rows;
        int w = img.cols;

        if (keep_aspect) {
            double scale = std::min(static_cast<double>(target_w) / w, 
                                   static_cast<double>(target_h) / h);
            int new_w = static_cast<int>(w * scale);
            int new_h = static_cast<int>(h * scale);
            cv::resize(img, img, cv::Size(new_w, new_h), 0, 0, interpolation);
        } else {
            cv::resize(img, img, cv::Size(target_w, target_h), 0, 0, interpolation);
        }
    }

    return *this;
}

void Img::draw_on(Img& other_img, int x, int y) {
    if (img.empty() || other_img.img.empty()) {
        throw std::runtime_error("Both images must be loaded before drawing.");
    }

    // Handle different channel counts
    cv::Mat source_img = img;
    cv::Mat target_img = other_img.img;
    
    if (source_img.channels() != target_img.channels()) {
        if (source_img.channels() == 3 && target_img.channels() == 4) {
            cv::cvtColor(source_img, source_img, cv::COLOR_BGR2BGRA);
        } else if (source_img.channels() == 4 && target_img.channels() == 3) {
            cv::cvtColor(source_img, source_img, cv::COLOR_BGRA2BGR);
        }
    }

    int h = source_img.rows;
    int w = source_img.cols;
    int H = target_img.rows;
    int W = target_img.cols;

    if (y + h > H || x + w > W) {
        throw std::runtime_error("Image does not fit at the specified position.");
    }

    cv::Mat roi = target_img(cv::Rect(x, y, w, h));

    if (source_img.channels() == 4) {
        // Handle alpha blending for BGRA images (per-pixel, not per-matrix-column)
        std::vector<cv::Mat> srcChannels;
        cv::split(source_img, srcChannels);

        cv::Mat alpha;
        srcChannels[3].convertTo(alpha, CV_32F, 1.0 / 255.0);

        std::vector<cv::Mat> dstChannels;
        cv::split(roi, dstChannels);

        for (int c = 0; c < 3; ++c) {
            cv::Mat srcF, dstF, blended;
            srcChannels[c].convertTo(srcF, CV_32F);
            dstChannels[c].convertTo(dstF, CV_32F);
            blended = srcF.mul(alpha) + dstF.mul(1.0f - alpha);
            blended.convertTo(dstChannels[c], CV_8U);
        }

        cv::merge(dstChannels, roi);
    } else {
        // Direct copy for BGR images
        source_img.copyTo(roi);
    }
}

void Img::put_text(const std::string& txt, int x, int y, double font_size,
                   const cv::Scalar& color, int thickness) {
    if (img.empty()) {
        throw std::runtime_error("Image not loaded.");
    }
    
    cv::putText(img, txt, cv::Point(x, y),
                cv::FONT_HERSHEY_SIMPLEX, font_size,
                color, thickness, cv::LINE_AA);
}

Img Img::clone() const {
    Img copy;
    copy.img = img.clone();
    return copy;
}

void Img::show() {
    if (img.empty()) {
        throw std::runtime_error("Image not loaded.");
    }
    
    cv::imshow("Image", img);
    cv::waitKey(0);
    cv::destroyAllWindows();
} 