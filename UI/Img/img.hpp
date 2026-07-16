#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <filesystem>

// A thin wrapper around cv::Mat: the one class in this project allowed to
// call OpenCV pixel-drawing functions directly. Every other UI class draws
// through this rather than touching cv::Mat itself.
class Img {
public:
    Img();
    
    /**
     * Load image from path and optionally resize.
     * 
     * @param path Image file to load
     * @param size Target size in pixels (width, height). If empty, keep original
     * @param keep_aspect If true, shrink so the longer side fits size while preserving aspect ratio
     * @param interpolation OpenCV interpolation flag (e.g., cv::INTER_AREA for shrink, cv::INTER_LINEAR for enlarge)
     * @return Reference to this object for method chaining
     */
    Img& read(const std::string& path,
              const std::pair<int, int>& size = {},
              bool keep_aspect = false,
              int interpolation = cv::INTER_AREA);

    /**
     * For images with no real alpha channel (plain RGB, background baked in
     * as white pixels): convert to BGRA and make every near-white pixel
     * fully transparent. A no-op for images that already have a real alpha
     * channel (e.g. a properly authored transparent PNG) - their existing
     * transparency is trusted as-is rather than overwritten by a guess.
     *
     * @param threshold Minimum B/G/R value (0-255) for a pixel to count as
     *                   "white" and be keyed out.
     */
    Img& keyOutNearWhite(int threshold = 240);

    /**
     * Replace this image with a blank canvas of the given size and color
     * (BGR). Used to build a board rendered entirely from code instead of a
     * background image file.
     */
    Img& create(int width, int height, const cv::Scalar& color = cv::Scalar(255, 255, 255));

    /**
     * Draw a filled rectangle directly onto this image.
     */
    Img& draw_rect(int x, int y, int w, int h, const cv::Scalar& color);

    /**
     * Draw an unfilled rectangle outline (a thin frame) directly onto this
     * image - used for square highlights, where draw_rect's solid fill
     * would hide whatever is drawn on the square.
     */
    Img& draw_rect_outline(int x, int y, int w, int h, const cv::Scalar& color, int thickness = 2);

    /**
     * Draw a circular arc (a portion of a circle's outline) directly onto
     * this image - used for radial progress indicators (e.g. a cooldown
     * meter). Angles are in degrees, measured clockwise from 3 o'clock,
     * matching cv::ellipse's own convention (so -90 is 12 o'clock).
     */
    Img& draw_arc(int centerX, int centerY, int radius, double startAngleDeg, double endAngleDeg,
                   const cv::Scalar& color, int thickness = 3);

    /**
     * Draw this image onto another image at position (x, y). If this image
     * has a real alpha channel (4 channels), it is always alpha-blended
     * into the target - even if the target has fewer channels - so a
     * transparent source never gets flattened into an opaque box. If not,
     * it's a plain copy (reformatted first if the channel counts differ).
     *
     * @param other_img The target image to draw on
     * @param x X coordinate for top-left corner
     * @param y Y coordinate for top-left corner
     */
    void draw_on(Img& other_img, int x, int y) const;
    
    /**
     * Put text on the image
     * 
     * @param txt Text to draw
     * @param x X coordinate for text position
     * @param y Y coordinate for text position (baseline)
     * @param font_size Font scale factor
     * @param color Text color (BGR or BGRA)
     * @param thickness Text thickness
     */
    void put_text(const std::string& txt, int x, int y, double font_size,
                  const cv::Scalar& color = cv::Scalar(255, 255, 255, 255),
                  int thickness = 1);

    /**
     * Pixel width `txt` would occupy if drawn with put_text at the given
     * font_size/thickness - for centering text without drawing it first.
     * Doesn't require an image to be loaded (pure text metrics).
     */
    int text_width(const std::string& txt, double font_size, int thickness = 1) const;

    /**
     * Display the image in a window
     */
    void show();

    /**
     * Return a deep copy of this image, so drawing on the copy leaves
     * the original untouched.
     */
    Img clone() const;

    /**
     * Get the underlying OpenCV Mat
     */
    const cv::Mat& get_mat() const { return img; }
    
    /**
     * Check if image is loaded
     */
    bool is_loaded() const { return !img.empty(); }

private:
    cv::Mat img;
}; 