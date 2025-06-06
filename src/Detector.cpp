//
// Created by davide on 03/10/23.
//

#include "Detector.h"
#include "utils.h"

Detector::Detector(const CalibrationInfo calib_info, const std::vector<CameraInfo>& camera_network_info, const int number_of_waypoints){

    calib_info_ = calib_info;
    camera_network_info_ = camera_network_info;
    number_of_waypoints_ = number_of_waypoints;
    pattern_size_ = cv::Size(calib_info_.getNumRow(), calib_info_.getNumCol());

    if (calib_info_.getPatternType() == "checkerboard") {
        std::cout << "The calibration pattern is the " << calib_info_.getPatternType() << ". " << std::endl;
    }
    else{
        throw std::invalid_argument("The calibration pattern \"" + calib_info_.getPatternType() + "\" is not accepted!");
    }
}

void Detector::getObjectPoints(std::vector<cv::Point3f> &objectPoints){

    if (calib_info_.getPatternType() == "checkerboard") {
        for (int i = 0; i < calib_info_.getNumCol(); ++i) {
            for (int j = 0; j < calib_info_.getNumRow(); ++j) {
                objectPoints.push_back(cv::Point3f(float(j * calib_info_.getSize()), float(i * calib_info_.getSize()), 0));
            }
        }
    }
}


// If you want to add other patterns, you can do it here by adding another function for the pattern detection
void Detector::patternDetection(std::vector<std::vector<cv::Mat>> images, std::vector<std::vector<cv::Mat>> poses, std::vector<std::vector<cv::Mat>> &correct_images, std::vector<std::vector<cv::Mat>> &correct_poses, std::vector<std::vector<std::vector<cv::Point2f>>> &correct_corners_only, std::vector<std::vector<std::vector<cv::Point2f>>> &correct_corners, std::vector<std::vector<int>> &cross_observation_matrix, std::vector<std::vector<cv::Mat>> &rvec_all, std::vector<std::vector<cv::Mat>> &tvec_all){

    if (calib_info_.getPatternType() == "checkerboard"){
        checkerboardDetection(images, poses, correct_images, correct_poses, correct_corners_only, correct_corners, cross_observation_matrix, rvec_all, tvec_all);
    }
}


void Detector::checkerboardDetection(std::vector<std::vector<cv::Mat>> images, std::vector<std::vector<cv::Mat>> poses, std::vector<std::vector<cv::Mat>> &correct_images, std::vector<std::vector<cv::Mat>> &correct_poses, std::vector<std::vector<std::vector<cv::Point2f>>> &correct_corners_only, std::vector<std::vector<std::vector<cv::Point2f>>> &correct_corners, std::vector<std::vector<int>> &cross_observation_matrix, std::vector<std::vector<cv::Mat>> &rvec_all, std::vector<std::vector<cv::Mat>> &tvec_all){

    std::cout << "Detecting corners.." << std::endl;
    for (int cam = 0; cam < calib_info_.getNumberOfCams(); cam++) {

        // Get the number of images for each camera
        int number_of_images = images[cam].size();

        // Start detection loop
        for (int img = 0; img < number_of_images; img++){

            showProgressBar(img, number_of_images);

            // Convert from BGR to gray image
            cv::Mat gray_image;
            cv::cvtColor(images[cam][img], gray_image, cv::COLOR_BGR2GRAY);

            // Corner detection
            std::vector<cv::Point2f> corners;
            bool pattern_found = cv::findChessboardCorners(gray_image, pattern_size_, corners,cv::CALIB_CB_ADAPTIVE_THRESH + cv::CALIB_CB_NORMALIZE_IMAGE + cv::CALIB_CB_FAST_CHECK );
            // Sub pixel accuracy
            if(pattern_found) {
                cv::cornerSubPix(gray_image, corners, cv::Size(11, 11), cv::Size(-1, -1), cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER, 30, 0.1));

                std::vector<cv::Point3f> object_points;
                getObjectPoints(object_points);

                // RANSAC filter
                cv::Mat rvec, tvec;
                bool correct_detection = filterRansac(object_points, corners, camera_network_info_[cam], rvec, tvec);
                if (correct_detection){
                    if (calib_info_.getVisualError())
                        correct_images[cam].push_back(images[cam][img].clone());
                    correct_poses[cam].push_back(poses[cam][img].clone());
                    correct_corners_only[cam].push_back(corners);

                    cross_observation_matrix[img][cam] = 1;
                    correct_corners[img][cam] = corners;
                    rvec_all[img][cam] = rvec;
                    tvec_all[img][cam] = tvec;
                }
            }
        }

        std::cout << "Camera " << std::to_string(cam+1) << ": process completed, " << std::to_string(correct_corners_only[cam].size()) << " filtered images!" << std::endl;
    }
}
