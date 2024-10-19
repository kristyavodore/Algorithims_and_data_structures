#include <gtest/gtest.h>
#include "client_logger_builder.h"


int main(){
    std::cout << "AAAAAAAA" << std::endl;
    auto *build_logger_1 = new client_logger_builder;
    build_logger_1 -> add_file_stream("1.txt",logger::severity::information);
    build_logger_1 -> set_format("%t %d %m %s");

    logger * logger_1 = build_logger_1 -> build();

    logger_1 -> log("lalalalalaaaaaa", logger::severity::information);

    delete build_logger_1;
    delete logger_1;

    std::cout << sizeof(unsigned char*)*300;
    return 0;
}