#include <not_implemented.h>
#include <map>
#include <filesystem>
#include <fstream>
#include"nlohmann/json.hpp"


#include "../include/client_logger_builder.h"
#include "client_logger.h"

class path;

class path;

client_logger_builder::client_logger_builder()=default;

client_logger_builder::client_logger_builder( //copy constructor
    client_logger_builder const &other)
{
    file_path_severity = other.file_path_severity;
    format_str = other.format_str;
}

client_logger_builder &client_logger_builder::operator=(
    client_logger_builder const &other){
    if (this != &other)
    {
        file_path_severity.clear();
        file_path_severity = other.file_path_severity;
        format_str = other.format_str;
    }
    return *this;
}


client_logger_builder::client_logger_builder( //move constructor
    client_logger_builder &&other) noexcept
{
    file_path_severity = std::move(other.file_path_severity);
    format_str = std::move(other.format_str);
}

client_logger_builder &client_logger_builder::operator=( //move assignment operator
    client_logger_builder &&other) noexcept
{
    if (this != &other)
    {
        //file_path_severity.clear();
        file_path_severity = std::move(other.file_path_severity);
        format_str = std::move(other.format_str);
    }
    return *this;
}

client_logger_builder::~client_logger_builder() noexcept //destructor
{
    file_path_severity.clear();
}

std::string client_logger_builder::absolut_path(const std::string &relative_path) // function for absolut path
{
    std::filesystem::path p = std::filesystem::absolute(relative_path);
    return p.string();
}

logger_builder *client_logger_builder::add_file_stream(
    std::string const &stream_file_path,
    logger::severity severity)
{
    if(stream_file_path.empty())
        throw std::logic_error("Path cant be empty");
    std::string stream_file_path_abs = absolut_path(stream_file_path);
    auto iter=file_path_severity.find(stream_file_path_abs);

    if (iter != file_path_severity.end()) // если не end, то элемент есть
    {
        iter->second |= (1 << static_cast<int>(severity));
    }
    else // элемента нет
    {
        file_path_severity.insert({stream_file_path_abs, (1 << static_cast<int>(severity))});
    }
return this;
}

logger_builder *client_logger_builder::add_console_stream(
    logger::severity severity)
{
    auto iter=file_path_severity.find("cerr");

    if (iter != file_path_severity.end()) // если не end, то элемент есть
    {
        iter->second |= (1 << static_cast<int>(severity));
    }
    else // элемента нет
    {
        file_path_severity.insert({"cerr", (1 << static_cast<int>(severity))});
    }
    return this;
}

logger_builder* client_logger_builder::transform_with_configuration(
        std::string const &configuration_file_path,
        std::string const &configuration_path)
{
    std::ifstream configuration_file(configuration_file_path);
    if(!configuration_file)
        throw std::runtime_error("File does not exist");

    nlohmann::json data;
    configuration_file >> data;

    for(const auto &file_path : data[configuration_path]) {
        std::string path = file_path["path"];
        auto severity = file_path["severity"];

        for(const std::string &s : severity)
            this->add_file_stream(path, string_to_severity(s));
    }

    this -> format_str = data["format"];

    configuration_file.close();

    return this;
}

logger_builder* client_logger_builder::set_format(std::string const& format)
{
    format_str = format;
    return this;
}

logger_builder *client_logger_builder::clear()
{
    file_path_severity.clear();
    format_str = " ";
    return this;
}

logger *client_logger_builder::build() const
{
    return new client_logger(file_path_severity, format_str);
}