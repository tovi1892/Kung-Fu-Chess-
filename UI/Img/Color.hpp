#pragma once

// A plain RGB(A) color with zero dependency on OpenCV - Img is the only place that
// knows how to turn this into whatever the underlying graphics library actually wants
// (cv::Scalar's B,G,R,A channel order today). Every other UI file describes colors in
// terms of this type, never cv::Scalar directly.
struct Color {
    int r = 0;
    int g = 0;
    int b = 0;
    int a = 255;  // fully opaque by default - most call sites never set this explicitly
};
