#pragma once

namespace kungfu {

// The one place that computes "how much bigger/smaller is the window than its reference
// size" - everything that needs to scale a pixel constant or font size for a resized
// OpenCvView window reads from this rather than inventing its own ratio. Pure arithmetic,
// zero OpenCV dependency, lives next to CoordinateMapper for the same reason: geometry the
// rest of the UI reads from, nobody computes their own.
//
// The factor is uniform (the smaller of the width/height ratios, not stretched
// independently) so square cells, circular rest-rings, and text never distort into ovals -
// a window resized to a different aspect ratio than the reference just leaves letterboxed
// space rather than warping anything.
class LayoutScale {
public:
    LayoutScale(int currentWidth, int currentHeight, int referenceWidth, int referenceHeight)
        : factor_(computeFactor(currentWidth, currentHeight, referenceWidth, referenceHeight)) {}

    // Scales a base/reference-size pixel constant to the current window size, rounding to
    // the nearest whole pixel.
    int px(int basePx) const { return static_cast<int>(basePx * factor_ + 0.5); }

    // Scales a base/reference-size font scale factor (as passed to cv::putText) the same way.
    double font(double baseSize) const { return baseSize * factor_; }

    double factor() const { return factor_; }

private:
    static double computeFactor(int currentWidth, int currentHeight, int referenceWidth, int referenceHeight) {
        const double widthRatio = static_cast<double>(currentWidth) / referenceWidth;
        const double heightRatio = static_cast<double>(currentHeight) / referenceHeight;
        return widthRatio < heightRatio ? widthRatio : heightRatio;
    }

    double factor_;
};

}  // namespace kungfu
