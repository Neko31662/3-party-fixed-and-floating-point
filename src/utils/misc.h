#pragma once
#include <cstdio>
#include <iostream>
#include <string>

void error(std::string msg) {
    std::cout<<"ERROR: " << msg << std::endl;
    exit(1);
}

void log(std::string msg) {
    fflush(stderr);
    fflush(stdout);
    std::cout << msg << std::endl;
    fflush(stdout);
}