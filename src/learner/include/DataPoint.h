/*
 * DataPoint.h
 *
 *  Created on: 5 Jul 2013
 *      Author: alex
 */

#ifndef DATAPOINT_H_
#define DATAPOINT_H_

#include <vector>

/**
 * This class acts as an interface that distance-based
 * machine learning algorithms will use (such as K-Nearest neighbour).
 */
class DataPoint
{

public:
	/**
	 * Given some other DataPoint, this function should calculate
	 * the distance between them.
	 *
	 * Returns: The distance between the two data points.
	 */
	virtual float getDistance(const DataPoint&) const = 0;

	/**
	 * Returns a vector of floats that define its position in n-dimensional
	 * space (n being the size of the returned vector)
	 *
	 * Returns: A vector of floats defining its position in n-dimensional space.
	 */
	virtual std::vector<float> getPosition() const = 0;

	/**
	 * Checks whether the two DataPoints are equal.
	 */
	bool operator==(const DataPoint& p){
		return getDistance(p) == 0;
	}

};

#endif /* DATAPOINT_H_ */
