/*
 * Project: Smart Document Scanner
 * Author: Gökalp Eker
 * Description:
 * A computer vision application that detects, crops, and digitizes physical documents
 * from images using OpenCV. It utilizes Canny Edge Detection, Convex Hulls for
 * robust shape detection, and Perspective Transformation matrices.
 * * Tech Stack: C++, OpenCV 4.x
 */
#include <iostream>
#include <opencv2/opencv.hpp>

using namespace cv;
using namespace std;


/**
 * Helper Function: Reorder Coordinates
 * -------------------------------------
 * Organizes a list of 4 points into a specific order:
 * [Top-Left, Top-Right, Bottom-Right, Bottom-Left]
 * This is crucial for the perspective transform to work correctly without mirroring.
 */
vector<Point> reorder(vector<Point> points) {
    vector<Point> newPoints(4);
    vector<int> sumPoints, diffPoints;

    for (const auto& pt : points) {
        sumPoints.push_back(pt.x + pt.y);
        diffPoints.push_back(pt.y - pt.x);
    }

    // TL
    newPoints[0] = points[min_element(sumPoints.begin(), sumPoints.end()) - sumPoints.begin()];
    // BR
    newPoints[2] = points[max_element(sumPoints.begin(), sumPoints.end()) - sumPoints.begin()];
    // TR
    newPoints[1] = points[min_element(diffPoints.begin(), diffPoints.end()) - diffPoints.begin()];
    // BL
    newPoints[3] = points[max_element(diffPoints.begin(), diffPoints.end()) - diffPoints.begin()];
    return newPoints;
}


int main(int argc, char** argv) {


    // 1. Load Image
    Mat img = imread("test.jpg", IMREAD_COLOR);
    if (img.empty()) {
        cerr << "Error: Could not load image from " << endl;
        return 1;
    }

    // Grayscale
    Mat gray;
    cvtColor(img, gray, COLOR_BGR2GRAY);


    // Gaussian Blur
    // Increased kernel size (5x5) to remove text details and focus on the document frame
    Mat blur;
    GaussianBlur(gray, blur, Size(5, 5), 0);

    // Canny Edge Detection
    // Canny thresholds adjusted for general lighting conditions
    Mat edges;
    Canny(blur, edges, 75, 200);


    // Dilation
    // Dilate helps to connect broken lines (e.g., book spines or faint edges)
    Mat kernel = getStructuringElement(MORPH_RECT, Size(5, 5));
    dilate(edges, edges, kernel);


    // Contour Finding & Selection
    vector<vector<Point>> contours;
    vector<Vec4i> hierarchy;


    findContours(edges, contours, hierarchy, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

    vector<Point> biggest;
    double maxArea = 0;
    vector<Point> hull; // Convex Hull

    for (const auto& cnt : contours) {
        double area = contourArea(cnt);

        // Filter out small noise
        if (area > 5000) {

            // Use Convex Hull to handle curved pages (like open books)
            convexHull(cnt, hull);

            double hullArea = contourArea(hull);

            if (hullArea > maxArea) {
                // Find the minimum bounding rectangle for the hull
                // This ensures we always get 4 corner points, even if the shape is irregular
                RotatedRect minRect = minAreaRect(hull);

                Point2f rect_points[4];
                minRect.points(rect_points);

                biggest.clear();
                for (int j = 0; j < 4; j++) {
                    biggest.push_back(rect_points[j]);
                }
                maxArea = hullArea;
            }
        }
    }

    // Warp Perspective (If a document is found)
    if (!biggest.empty()) {
        // Reorder points to match standard orientation
        biggest = reorder(biggest);

        float w = 1000;
        float h = 1414;

        vector<Point2f> dst = {
                {0, 0},   {w, 0},
                {w, h},   {0, h}
        };

        // Define source and destination points
        vector<Point2f> src;
        for(auto pt : biggest) src.push_back(Point2f(pt.x, pt.y));

        // Calculate Transformation Matrix
        Mat matrix = getPerspectiveTransform(src, dst);
        Mat imgWarp;
        warpPerspective(img, imgWarp, matrix, Point(w, h));


        // Post-Processing (Adaptive Thresholding)
        // Creates a clean, scanned look by binarizing the image locally
        Mat imgCroppedGray, imgThreshold;

        // Önce kesilmiş resmi griye çevir
        cvtColor(imgWarp, imgCroppedGray, COLOR_BGR2GRAY);

        // Adaptive Threshold: Eliminates light and shadow differences, making only the text black.
        // 255: Maximum color (white)
        // ADAPTIVE_THRESH_GAUSSIAN_C: Determines by looking at neighboring pixels
        // 21: Neighborhood size (must be an odd number)
        // 5: Fixed number (noise adjustment)
        adaptiveThreshold(imgCroppedGray, imgThreshold, 255, ADAPTIVE_THRESH_GAUSSIAN_C, THRESH_BINARY, 21, 5);

        // Display Results
        imshow("Original Doc", img);
        imshow("Scanned Doc", imgWarp);
        imshow("Scanned and cleaned Doc", imgThreshold);
        cout << "Document scanned successfully!" << endl;

    } else {
        cout << "No document found. Try a background with higher contrast." << endl;
    }
    waitKey(0);
    return 0;
}