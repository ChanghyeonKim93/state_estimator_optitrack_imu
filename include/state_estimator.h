#ifndef _STATE_ESTIMATOR_H_
#define _STATE_ESTIMATOR_H_

#include <iostream>
#include <string>

#include <ros/ros.h>
#include <sensor_msgs/Imu.h> // Imu (acc & gyro)
#include <sensor_msgs/MagneticField.h> // Imu magnetometer

#include <geometry_msgs/PoseStamped.h> // optitrack
#include <Eigen/Dense>

using namespace Eigen;

class StateEstimator{
// Callback functions
private:
	void callbackOptitrack(const geometry_msgs::PoseStampedConstPtr& msg);
	void callbackIMU(const sensor_msgs::ImuConstPtr& msg);
	void callbackMag(const sensor_msgs::MagneticFieldConstPtr& msg);

    void run();

public:
    StateEstimator(ros::NodeHandle& nh);
    ~StateEstimator();
    
private:
    ros::NodeHandle nh_;

    std::string topicname_imu_;
    std::string topicname_mag_;
    std::string topicname_optitrack_;

	// subscribers
	ros::Subscriber sub_imu_;
	ros::Subscriber sub_mag_;
	ros::Subscriber sub_optitrack_;

    // publishers
    ros::Publisher pub_pose_;

// Estimated Pos / Linear Velocity / Quadternion / Angular velocity 
private:
    float p_est_[3]; // global frame
    float v_est_[3]; // global frame 
    float w_est_[3]; // body frame
    float q_est_[4]; // global frame

};
#endif