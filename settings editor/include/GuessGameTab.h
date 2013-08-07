/*
 * GuessGameTab.h
 *
 *  Created on: 5 Aug 2013
 *      Author: rapid
 */

#ifndef GUESSGAMETAB_H_
#define GUESSGAMETAB_H_

#include <QTabWidget>
#include <QString>

class GuessGameTab : public QTabWidget
{

public:
	static const QString TAB_NAME;

	GuessGameTab() : QTabWidget()
	{
		init();
	}

private:
	void init();

};

#endif /* GUESSGAMETAB_H_ */
