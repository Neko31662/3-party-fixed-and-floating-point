#pragma once
#include <cstdio>
#include <iostream>
#include <string>

void error(const char *msg) {
    printf("ERROR: %s\n", msg);
    exit(1);
}

void log(std::string msg) {
    fflush(stderr);
    fflush(stdout);
    std::cout << msg << std::endl;
    fflush(stdout);
}