#include <QApplication>
#include <QPlastiqueStyle>

#include <QWidget>

#include <Window.h>
#include <json/json.h>
#include <fstream>

#include <boost/shared_ptr.hpp>

Json::Value loadJson();

int main(int argc, char** argv)
{
	QApplication app(argc, argv);
	app.setStyle(new QPlastiqueStyle);

	Json::Value json = loadJson();
	//Controller* controller = new Controller(model);

	boost::shared_ptr<Model> modelPtr(new Model);
	boost::shared_ptr<Controller> controllerPtr(new Controller(modelPtr.get()));

	Window window(controllerPtr, modelPtr);

	//modelPtr.get()->loadData(json);

	return app.exec();
}

Json::Value loadJson(){
	char* fileName = "data.json";

	std::string jsonData;

	std::fstream ifs;
	ifs.open(fileName, std::fstream::in);

	ifs.seekg(0, std::ios::end);
	jsonData.resize(ifs.tellg());
	ifs.seekg(0, std::ios::beg);

	ifs.read(&jsonData[0], jsonData.size());
	ifs.close();

	Json::Reader jsonReader;

	Json::Value doc;
	jsonReader.parse(jsonData, doc);

	return doc;
}