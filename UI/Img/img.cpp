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

Img& Img::create(int width, int height, const cv::Scalar& color) {
    img = cv::Mat(height, width, CV_8UC3, color);
    return *this;
}

Img& Img::draw_rect(int x, int y, int w, int h, const cv::Scalar& color) {
    if (img.empty()) {
        throw std::runtime_error("Image not loaded.");
    }
    cv::rectangle(img, cv::Rect(x, y, w, h), color, cv::FILLED);
    return *this;
}

Img& Img::keyOutNearWhite(int threshold) {
    if (img.empty()) {
        return *this;
    }

    if (img.channels() == 4) {
        return *this;  // already has real alpha - trust it, don't guess
    }
    if (img.channels() != 3) {
        return *this;  // grayscale or unexpected format - nothing sensible to key out
    }

    cv::cvtColor(img, img, cv::COLOR_BGR2BGRA);

    for (int y = 0; y < img.rows; ++y) {
        auto* row = img.ptr<cv::Vec4b>(y);
        for (int x = 0; x < img.cols; ++x) {
            cv::Vec4b& px = row[x];
            const bool isNearWhite = px[0] >= threshold && px[1] >= threshold && px[2] >= threshold;
            px[3] = isNearWhite ? 0 : 255;
        }
    }

    return *this;
}

void Img::draw_on(Img& other_img, int x, int y) const {
    if (img.empty() || other_img.img.empty()) {
        throw std::runtime_error("Both images must be loaded before drawing.");
    }

    const cv::Mat& source_img = img;
    cv::Mat& target_img = other_img.img;

    int h = source_img.rows;
    int w = source_img.cols;
    int H = target_img.rows;
    int W = target_img.cols;

    if (y + h > H || x + w > W) {
        throw std::runtime_error("Image does not fit at the specified position.");
    }

    cv::Mat roi = target_img(cv::Rect(x, y, w, h));

    if (source_img.channels() == 4) {
        // Alpha-blend regardless of the target's channel count - a source
        // with real transparency must never be flattened before blending,
        // or its background stops being transparent.
        std::vector<cv::Mat> srcChannels;
        cv::split(source_img, srcChannels);

        cv::Mat alpha;
        srcChannels[3].convertTo(alpha, CV_32F, 1.0 / 255.0);

        std::vector<cv::Mat> dstChannels;
        cv::split(roi, dstChannels);

        const int blendChannels = std::min<size_t>(3, dstChannels.size());
        for (int c = 0; c < blendChannels; ++c) {
            cv::Mat srcF, dstF, blended;
            srcChannels[c].convertTo(srcF, CV_32F);
            dstChannels[c].convertTo(dstF, CV_32F);
            blended = srcF.mul(alpha) + dstF.mul(1.0f - alpha);
            blended.convertTo(dstChannels[c], CV_8U);
        }

        cv::merge(dstChannels, roi);
    } else if (source_img.channels() == target_img.channels()) {
        source_img.copyTo(roi);
    } else {
        // A source with no alpha of its own, but a differing channel count
        // from the target - just reformat and copy (nothing to blend).
        cv::Mat converted;
        if (source_img.channels() == 3 && target_img.channels() == 4) {
            cv::cvtColor(source_img, converted, cv::COLOR_BGR2BGRA);
        } else {
            cv::cvtColor(source_img, converted, cv::COLOR_BGRA2BGR);
        }
        converted.copyTo(roi);
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