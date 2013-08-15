/*
 * NaoMimicker.cpp
 *  Created on: Aug 5, 2013
 *      Author: Tristan Bell
 *
 * Notes on joint correspondence (minus denotes inverse):
 * 		Kinect Data			Nao
 * 		--------------		-------------
 * 		LShoudler Roll		LShoulder Roll
 * 		LShoudler -Yaw		LShoulder Pitch
 * 		LElbow -Pitch		LElbow Yaw
 * 		LElbow -Yaw			LElbow Roll
 *
 * 		RShoudler -Roll		RShoulder Roll
 * 		RShoudler Yaw		RShoulder Pitch
 * 		RElbow -Pitch		RElbow Yaw
 * 		RElbow -Yaw			RElbow Roll
 *
 *
 * Joint name		Motion									Range (degrees)		Range (radians)
 * --------------	-------------------------------------	---------------		---------------
 * LShoulderPitch	Left shoulder joint front and back (Y)	-119.5 to 119.5		-2.0857 to 2.0857
 * LShoulderRoll	Left shoulder joint right and left (Z)	-18 to 76			-0.3142 to 1.3265
 * LElbowYaw		Left shoulder joint twist (X)			-119.5 to 119.5		-2.0857 to 2.0857
 * LElbowRoll		Left elbow joint (Z)					-88.5 to -2			-1.5446 to -0.0349
 *
 * RShoulderPitch	Right shoulder joint front and back (Y)	-119.5 to 119.5		-2.0857 to 2.0857
 * RShoulderRoll	Right shoulder joint right and left (Z)	-76 to 18			-1.3265 to 0.3142
 * RElbowYaw		Right shoulder joint twist (X)			-119.5 to 119.5		-2.0857 to 2.0857
 * RElbowRoll		Right elbow joint (Z)					2 to 88.5			0.0349 to 1.5446
 */

// ROS and relevant messages
#include <ros/ros.h>
#include <tf/tfMessage.h>
#include <tf/tf.h>
#include <nao_msgs/JointAnglesWithSpeed.h>
#include <visualization_msgs/Marker.h>
#include <std_msgs/String.h>
#include <nao_control/NaoControl.h>

#include <vector>
#include <string>

#define TRACKER_FRAME "/openni_depth_frame"
#define CONTROL_DIST 0.3

/*
 * Used to determine the direction the Nao should walk in,
 * according to the positions of limbs.
 */
enum RobotMovements {
	FORWARD,
	BACKWARD,
	LEFT,
	RIGHT,
	STOPX,
	STOPY
};

enum NaoPose {
	SITTING,
	STANDING
};

/*
 * Stores a whole user pose (only relevant joints).
 */
struct Pose {
	tf::Vector3* leftHand;
	tf::Vector3* leftElbow;
	tf::Vector3* leftShoulder;
	tf::Vector3* rightHand;
	tf::Vector3* rightElbow;
	tf::Vector3* rightShoulder;
	tf::Vector3* leftHip;
	tf::Vector3* rightHip;
	tf::Vector3* leftFoot;
	tf::Vector3* rightFoot;
};

/*
 *  Represents the bounding box around the user which allows
 *  the user to control the Nao's walking movements.
 */
struct ControlBox {
	double front, back, left, right;

	ControlBox(geometry_msgs::Vector3 userPosition)
	{
		front = userPosition.x - CONTROL_DIST;
		back = userPosition.x + CONTROL_DIST;
		left = userPosition.y - CONTROL_DIST;
		right = userPosition.y + CONTROL_DIST;
	}
};

// For initial setup, determining which user will control the Nao
std::string _userNumber = "";
ros::Publisher _user_pub;
// Links user numbers to hand and head positions
std::map< std::string, std::pair<geometry_msgs::Vector3, geometry_msgs::Vector3> > _userDistanceMap;

// For controlling the Nao
nao_control::NaoControl* _nao_control;
NaoPose _naoPose;
ControlBox *_controlBox;
RobotMovements _leftXMotionControl;
RobotMovements _leftYMotionControl;
RobotMovements _rightXMotionControl;
RobotMovements _rightYMotionControl;
ros::Publisher _arms_pub;
ros::Publisher _walk_pub;
bool broadcasting;
bool ready;

// Positions of joints to calculate elbow roll
Pose _pose;
float _leftElbowRoll = 0.0;
float _rightElbowRoll = 0.0;

// Torso rotation used to turn the robot
double _initTorsoRotation, _torsoRotation = 0.0;
#define ROTATION_BOUNDARY 0.1

/**
 * Initialises torso rotation, leg movement and a ControlBox around the user.
 */
void init(geometry_msgs::TransformStamped initTransform) {
	// Initialise torso rotation
	float x = initTransform.transform.rotation.x;
	float y = initTransform.transform.rotation.y;
	float z = initTransform.transform.rotation.z;
	float w = initTransform.transform.rotation.w;
	tf::Quaternion quat(x, y, z, w);
	tf::Matrix3x3 mat(quat);
	double roll, pitch, yaw;
	mat.getRPY(roll, pitch, yaw);
	_initTorsoRotation = _torsoRotation = pitch;

	geometry_msgs::Vector3 userPosition = initTransform.transform.translation;
	_controlBox = new ControlBox(userPosition);

	// Initialise leg movement to stop
	_leftXMotionControl = STOPX;
	_rightXMotionControl = STOPX;
	_leftYMotionControl = STOPY;
	_rightYMotionControl = STOPY;

	_nao_control = new nao_control::NaoControl();
	_naoPose = STANDING;
}

/**
 * Checks to make sure openni_depth_frame is broadcasting.
 */
void checkTfBroadcasting(geometry_msgs::TransformStamped transform)
{
	if (transform.header.frame_id == TRACKER_FRAME
			&& transform.child_frame_id.find("left_foot") != std::string::npos) {
		ROS_INFO("/tf is now publishing, ready to go.");
		broadcasting = true;
	}
}

/**
 * Creates the nao_msgs::JointAnglesWithSpeed msg that corresponds to
 * the given vector of joints and angles.
 */
void constructAndPublishMsg(std::vector<std::string> &joints, std::vector<float> &angles)
{
	nao_msgs::JointAnglesWithSpeed jointMsg;
	jointMsg.speed = 0.28;
	jointMsg.joint_names = joints;
	jointMsg.joint_angles = angles;

	_arms_pub.publish(jointMsg);
}

/**
 * Calculates which direction the Nao should move, then constructs and
 * publishes the corresponding geometry_msgs::Twist msg.
 */
void constructAndPublishMovement(void) {
	geometry_msgs::Twist msg;

//	std::cout << "Movement: ";

	if (_leftXMotionControl == FORWARD || _rightXMotionControl == FORWARD) {
//		std::cout << "forward  " << "\r";
		msg.linear.x = 1;
	} else if (_leftXMotionControl == BACKWARD || _rightXMotionControl == BACKWARD) {
//		std::cout << "backward " << "\r";
		msg.linear.x = -1;
	}
	if (_leftXMotionControl == STOPX && _rightXMotionControl == STOPX) {
//		std::cout << "stop    " << "\r";
		msg.linear.x = 0;
	}

	// Only move left or right if not already moving forward or back
	if (msg.linear.x == 0) {
		if (_leftYMotionControl == LEFT) {
//			std::cout << "left     " << "\r";
			msg.linear.y = 1;
		} else if (_rightYMotionControl == RIGHT) {
//			std::cout << "right    " << "\r";
			msg.linear.y = -1;
		}

		if (_leftYMotionControl == STOPY && _rightYMotionControl == STOPY) {
//			std::cout << "stopY" << std::endl;
			msg.linear.y = 0;
		}
	} else {
		msg.linear.y = 0;
	}

	if (_torsoRotation > _initTorsoRotation + ROTATION_BOUNDARY) {
		// Turn right
//		std::cout << "Turn right      " << "\r";
		msg.angular.z = 1;
	}
	else if (_torsoRotation < _initTorsoRotation - ROTATION_BOUNDARY) {
		// Turn left
//		std::cout << "Turn left       " << "\r";
		msg.angular.z = -1;
	}

	std::flush(std::cout);

	_walk_pub.publish(msg);
}

/**
 * Gets the angle in radians between three connected joints.
 */
float getJointAngle(tf::Vector3* outer, tf::Vector3* middle, tf::Vector3* inner)
{
	if (middle && outer && inner) {
		tf::Vector3 middleToOuter;
		tf::Vector3 innerToMiddle;

		float etohLength = middle->distance(*outer);
		float stoeLength = inner->distance(*middle);

		// Find distances and normalize (to between 0 and 1)
		middleToOuter.setX((middle->x() - outer->x()) / etohLength);
		middleToOuter.setY((middle->y() - outer->y()) / etohLength);
		middleToOuter.setZ((middle->z() - outer->z()) / etohLength);

		innerToMiddle.setX((inner->x() - middle->x()) / stoeLength);
		innerToMiddle.setY((inner->y() - middle->y()) / stoeLength);
		innerToMiddle.setZ((inner->z() - middle->z()) / stoeLength);

		float theta = middleToOuter.angle(innerToMiddle);

		return theta;
	}

	return -100.0; // Error value
}

/**
 * Gets the angle in radians between two (unconnected) limbs.
 * For example, left elbow yaw can be calculated by
 * getLimbAngle(leftHand, leftElbow, leftShoulder, leftHip)
 */
float getLimbAngle(tf::Vector3* joint1, tf::Vector3* joint2, tf::Vector3* joint3, tf::Vector3* joint4)
{
	if (joint1 && joint2 && joint3 && joint4) {
		tf::Vector3 limb1;
		tf::Vector3 limb2;

		float etohLength = joint1->distance(*joint2);
		float stoeLength = joint3->distance(*joint4);

		// Find distances and normalize (to between 0 and 1)
		limb1.setX((joint1->x() - joint2->x()) / etohLength);
		limb1.setY((joint1->y() - joint2->y()) / etohLength);
		limb1.setZ((joint1->z() - joint2->z()) / etohLength);

		limb2.setX((joint3->x() - joint4->x()) / stoeLength);
		limb2.setY((joint3->y() - joint4->y()) / stoeLength);
		limb2.setZ((joint3->z() - joint4->z()) / stoeLength);

		float theta = limb1.angle(limb2);

		return theta;
	}

	return -100.0; // Error value
}

/**
 * Determines leg movement based on the user's position.
 */
void moveLegs(geometry_msgs::TransformStamped& transform,
		geometry_msgs::Vector3& translation)
{
	std::string child = transform.child_frame_id;
	if (child == "/left_foot" + _userNumber) {
		geometry_msgs::Twist msg;
		if (translation.x < _controlBox->front) {
			_leftXMotionControl = FORWARD;
		} else if (translation.x > _controlBox->back) {
			_leftXMotionControl = BACKWARD;
		} else {
			_leftXMotionControl = STOPX;
		}

		if (translation.y > _controlBox->right) {
			_leftYMotionControl = LEFT;
		} else {
			_leftYMotionControl = STOPX;
		}
	}

	if (child == "/right_foot" + _userNumber) {
		if (translation.x < _controlBox->front) {
			_rightXMotionControl = FORWARD;
		} else if (translation.x > _controlBox->back) {
			_rightXMotionControl = BACKWARD;
		} else {
			_rightXMotionControl = STOPX;
		}

		if (translation.y < _controlBox->left) {
			_rightYMotionControl = RIGHT;
		} else {
			_rightYMotionControl = STOPY;
		}
		constructAndPublishMovement();
	}

	if (child == "/torso" + _userNumber) {
		float x = transform.transform.rotation.x;
		float y = transform.transform.rotation.y;
		float z = transform.transform.rotation.z;
		float w = transform.transform.rotation.w;
		tf::Quaternion quat(x, y, z, w);
		tf::Matrix3x3 mat(quat);
		double roll, pitch, yaw;
		mat.getRPY(roll, pitch, yaw);

		_torsoRotation = pitch;
	}
}

/**
 * Determines arm movement given the user's arm positions.
 */
void moveArms(geometry_msgs::TransformStamped& transform,
		geometry_msgs::Vector3& translation)
{
	bool toPublish = false;

	tf::Vector3 origin(translation.x, translation.y, translation.z);
	float roll, pitch, yaw;

	// Vectors for publishing to Nao (ex. joint at joints[i] moves to angles[i] angle)
	std::vector < std::string > joints;
	std::vector<float> angles;

	if (transform.child_frame_id.find("left_shoulder" + _userNumber) != std::string::npos) {
		_pose.leftShoulder = new tf::Vector3(origin);
		pitch = getJointAngle(_pose.leftElbow, _pose.leftShoulder, _pose.leftHip) - 1.6;
		roll = 1.63 - getLimbAngle(_pose.leftElbow, _pose.leftShoulder, _pose.leftHip, _pose.rightHip);

		if (pitch > -2.0857 && pitch < 2.0857) {
			joints.push_back("LShoulderPitch");
			angles.push_back((float) pitch);
			toPublish = true;
		}
		if (roll > -0.3142 && roll < 1.3265) {
			joints.push_back("LShoulderRoll");
			angles.push_back((float) roll);
			toPublish = true;
		}
	}

	if (transform.child_frame_id.find("right_shoulder" + _userNumber)
			!= std::string::npos) {
		_pose.rightShoulder = new tf::Vector3(origin);
		pitch = getJointAngle(_pose.rightElbow, _pose.rightShoulder, _pose.rightHip) - 1.6;
		roll = getLimbAngle(_pose.rightElbow, _pose.rightShoulder, _pose.rightHip, _pose.leftHip) - 1.57;

		if (pitch > -2.0857 && pitch < 2.0857) {
			joints.push_back("RShoulderPitch");
			angles.push_back((float) pitch);
			toPublish = true;
		}
		if (roll > -1.3265 && roll < 0.3142) {
			joints.push_back("RShoulderRoll");
			angles.push_back((float) roll);
			toPublish = true;
		}
	}

	if (transform.child_frame_id.find("left_elbow" + _userNumber) != std::string::npos) {
		_pose.leftElbow = new tf::Vector3(origin);
		yaw = getLimbAngle(_pose.leftHand, _pose.leftElbow, _pose.leftShoulder, _pose.leftHip) - 1.57;
		roll = -getJointAngle(_pose.leftHand, _pose.leftElbow, _pose.leftShoulder);

		if (yaw > -2.0857 && yaw < 2.0857) {
			joints.push_back("LElbowYaw");
			angles.push_back((float) (yaw));
			toPublish = true;
		}
		if (roll > -1.5446 && roll < -0.0349) {
			joints.push_back("LElbowRoll");
			angles.push_back((float) (roll));
			toPublish = true;
		}
	}

	if (transform.child_frame_id.find("right_elbow" + _userNumber) != std::string::npos) {
		_pose.rightElbow = new tf::Vector3(origin);
		yaw = 1.57 - getLimbAngle(_pose.rightHand, _pose.rightElbow, _pose.rightShoulder, _pose.rightHip);
		roll = getJointAngle(_pose.rightHand, _pose.rightElbow, _pose.rightShoulder);

		if (yaw > -2.0857 && yaw < 2.0857) {
			joints.push_back("RElbowYaw");
			angles.push_back((float) (yaw));
			toPublish = true;
		}
		if (roll > 0.0349 && roll < 1.5446) {
			joints.push_back("RElbowRoll");
			angles.push_back((float) (roll));
			toPublish = true;
		}
	}

	if (transform.child_frame_id.find("left_hand" + _userNumber) != std::string::npos) {
		_pose.leftHand = new tf::Vector3(origin);
	}
	if (transform.child_frame_id.find("right_hand" + _userNumber) != std::string::npos) {
		_pose.rightHand = new tf::Vector3(origin);
	}
	if (transform.child_frame_id.find("left_hip" + _userNumber) != std::string::npos) {
		_pose.leftHip = new tf::Vector3(origin);
	}
	if (transform.child_frame_id.find("right_hip" + _userNumber) != std::string::npos) {
		_pose.rightHip = new tf::Vector3(origin);
	}
	if (transform.child_frame_id.find("left_foot" + _userNumber) != std::string::npos) {
		_pose.leftFoot = new tf::Vector3(origin);
	}
	if (transform.child_frame_id.find("right_foot" + _userNumber) != std::string::npos) {
		_pose.rightFoot = new tf::Vector3(origin);
	}

	if (toPublish) {
		constructAndPublishMsg(joints, angles);
	}
}

#define MIN_DISTANCE 0.5

/**
 * Looks for a gesture (right hand in front of head) to determine which user to track.
 * When the gesture is found, it will initialise the control box around the user
 * and they will be able to control the Nao.
 */
void lookForStartGesture(const geometry_msgs::TransformStamped& transform,
						const geometry_msgs::Vector3& translation)
{
	std::string child = transform.child_frame_id;
	std::string userNum = child.substr(child.size() - 2);

	// Put user numbers and joint positions in a map to keep track of all users
	if (child.find("head") != std::string::npos) {
		_userDistanceMap[userNum].first = translation;
	}
	if (child.find("left_hand") != std::string::npos) {
		_userDistanceMap[userNum].second = translation;
	}

	try {
		// Use torso frame for initialising control box
		if (child.find("torso") != std::string::npos) {
			geometry_msgs::Vector3 head = _userDistanceMap.at(userNum).first;
			geometry_msgs::Vector3 hand = _userDistanceMap.at(userNum).second;

			// Lazy distance calculation
			tf::Vector3 he(head.x, head.y, head.z);
			tf::Vector3 ha(hand.x, hand.y, hand.z);
			float dist = he.distance(ha);

			if (dist <= MIN_DISTANCE) {
				_userNumber = userNum;

				// Signal to TfRemap node which user to track
				std_msgs::String msg;
				msg.data = _userNumber;
				_user_pub.publish(msg);

				printf("User set: %s           \n", userNum.c_str());
				init(transform);
				ready = true;
			}
		}
	} catch (std::out_of_range& ex) { }
}

#define SITTING_DISTANCE 0.4

void checkSitting(void)
{
	if (_pose.leftHip && _pose.leftFoot && _pose.rightHip && _pose.rightFoot) {
		float lDist = _pose.leftHip->distance(*_pose.leftFoot);
		float rDist = _pose.rightHip->distance(*_pose.rightFoot);

		if (_naoPose == STANDING && (lDist < SITTING_DISTANCE || rDist < SITTING_DISTANCE)) {
//			if (_nao_control.perform("sit_down"))
				_naoPose = SITTING;
		}
		else if (_naoPose == SITTING && lDist >= SITTING_DISTANCE && rDist >= SITTING_DISTANCE) {
//			if (_nao_control.perform("stand_up"))
				_naoPose = STANDING;
		}

		printf("%s      %f, %f     \r", (_naoPose == SITTING ? "SITTING" : "STANDING"), lDist, rDist);
		std::flush(std::cout);
	}
}

/**
 * All-encompassing callback method for tf messages. Performs initial broadcasting
 * check to make sure this node is receiving openni transforms, then looks for
 * which user to track. Once this is done, it moves the robot according to the
 * user's movements.
 */
void tfCallback(const tf::tfMessage msg)
{
	geometry_msgs::TransformStamped transform = msg.transforms[0];
	geometry_msgs::Vector3 translation = transform.transform.translation;

	if (broadcasting) {
		if (ready) {
			// Move arms
			if (transform.header.frame_id == TRACKER_FRAME) {
				moveArms(transform, translation);
				moveLegs(transform, translation);
				checkSitting();
			}
		} else {
			if (transform.header.frame_id == TRACKER_FRAME) {
				lookForStartGesture(transform, translation);
			}
		}
	} else {
		// Only allow application to continue when message containing tf for head joint is received.
		checkTfBroadcasting(transform);
	}
}

int main(int argc, char **argv) {
	ros::init(argc, argv, "mimicker");
	ros::NodeHandle nh;
	_arms_pub = nh.advertise<nao_msgs::JointAnglesWithSpeed>("/joint_angles", 50);
	_walk_pub = nh.advertise<geometry_msgs::Twist>("/cmd_vel", 2);
	_user_pub = nh.advertise<std_msgs::String>("/user_number", 1);
	ros::Subscriber sub = nh.subscribe("/tf", 50, tfCallback);

	ros::Rate rate(60);
	ROS_INFO("Waiting for a head tf to be published.");
	while (!broadcasting) {
		ros::spinOnce();

		if (!nh.ok()) {
			ROS_INFO("Closing down.");

			exit(EXIT_FAILURE);
		}

		rate.sleep();
	}

	ROS_INFO("All set up, go!");
	ros::spin();

	// Send message to stop moving once the program has finished
	geometry_msgs::Twist stopMsg;
	stopMsg.linear.x = 0.0;
	stopMsg.linear.y = 0.0;
	_walk_pub.publish(stopMsg);

	return 0;
}



