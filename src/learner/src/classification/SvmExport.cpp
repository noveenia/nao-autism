/**
 * SvmExport.cpp
 *
 * This node takes rosbag files of tf data and their corresponding classifications
 * and exports that data in a way that can be interpreted by LibSVM.
 */

#include <classification/Learner.h>
#include <classification/KNearestNeighbour.h>
#include <classification/DWKNearestNeighbour.h>
#include <classification/DataLoader.h>
#include <classification/DataStore.h>
#include <classification/PlainDataStore.h>
#include <classification/DataPoint.h>

#include <nao_autism_messages/PoseClassification.h>

#include <vector>
#include <map>
#include <exception>
#include <iostream>
#include <fstream>

#include <tf/tfMessage.h>
#include <geometry_msgs/TransformStamped.h>

#include <ros/ros.h>
#include <std_msgs/Int32.h>

int main(int argc, char** argv)
{
	if ((argc % 2) == 1 && argc > 1){
		ros::init(argc, argv, "svm_export");

		classification::TrainingData classifiedPoints;

		ROS_INFO("Scanning argument(s)");
		for (unsigned int i=1;i<argc;i+=2){
			std::string fileName(argv[i]);
			int classification = strtol(argv[i+1], NULL, 10);

			std::vector<classification::DataPoint*> dataPoints = classification::DataLoader::loadData(fileName);
			for (unsigned int j=0;j<dataPoints.size();j++){
				classification::DataPoint* current = dataPoints[j];

				current->setClassification(classification);
				classifiedPoints.push_back(current);
			}
		}

		ROS_INFO("Writing to file");

		using namespace std;

		ofstream out("nao_autism3");

		vector<classification::PoseDataPoint*> pdata =
				classification::PoseDataPoint::convertToPoses(classifiedPoints);

		BOOST_FOREACH(classification::PoseDataPoint* dataPoint, pdata){
			// Classification will be the index, so written first
			out << "+" << dataPoint->getClassification() << " ";

			vector<geometry_msgs::TransformStamped> joints = dataPoint->poseData.getJoints();

			int indexCount = 1;

			for (int i = 0; i < joints.size(); i++) {
				out << indexCount++ << ":" << joints[i].transform.rotation.x << " ";
				out << indexCount++ << ":" << joints[i].transform.rotation.y << " ";
				out << indexCount++ << ":" << joints[i].transform.rotation.z << " ";
				out << indexCount++ << ":" << joints[i].transform.rotation.w << " ";
			}

			out << "\n";
		}

		out.close();

		ROS_INFO("Done.");
	} else {
		ROS_INFO("Invalid arguments, The arguments should be supplied in pairs of <filename> and <classification>");
		ROS_INFO("Where the filename is the name of the bag file containing solely tf transforms and the classification is some short integer.");

		return -1;
	}

	return 0;
}
