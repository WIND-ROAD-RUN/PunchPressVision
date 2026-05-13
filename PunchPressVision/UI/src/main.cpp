#include <iostream>

#include "halconcpp/HalconCpp.h"

#include <QApplication>

int main(int argc, char* argv[]){
    QApplication a(argc, argv);

    std::cout << "Hello, World!" << std::endl;
    HalconCpp::HObject ho_Image;

    return a.exec();
}