/*
 * recorder2.h
 *
 *  Created on: 2 Jul 2013
 *      Author: tristan
 */

#ifndef RECORDER_H_
#define RECORDER_H_

#include <rosbag/recorder.h>
#include <ros/ros.h>
#include <nao_autism_messages/Record.h>
#include <vector>
#include <string>

class Recorder {
public:
	static void recordCallback(const nao_autism_messages::Record::ConstPtr& msg);
	static void record(void);
	void stop(void); // Unimplemented
	static const float RECORDING_DURATION;
	static const uint32_t RECORDING_SIZE;

private:
	Recorder(void);
	rosbag::RecorderOptions options;
};

#endif /* RECORDER_H_ */
