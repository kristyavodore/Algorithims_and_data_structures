#include <not_implemented.h>
#include <iostream>
#include <fstream>

#include "../include/client_logger.h"

client_logger::client_logger(std::map <std::string, unsigned char> path_severity, std::string format){
    for (auto i=0; i < format.size();  i++)
    {
        if (format[i] == '%')
        {
            if (i == format.size()-1)
            {
                throw std::logic_error("Format is invalid");
            }
            if (!isalpha(format[i+1])){
                throw std::logic_error("Format is invalid");
            }
            format_str+=format[i];
        }
    }

    for (auto const &pair: path_severity)
    {
        if (pair.first != "cerr")
        {
            auto a = map_streams.find(pair.first);

            if (a != map_streams.end())
            {
                a->second.second+1;
            }
            else
            {
                //std::ofstream* file = new std::ofstream(pair.first);
                a->second.first = new std::ofstream(pair.first);
                if (!a->second.first) { // если текущий файл не открылся
                    a->second.first->close();
                    delete a->second.first;

                    for (auto const &p_streams: map_streams) { // то закрываем уже открытые файлы и чистим указатели
                        p_streams.second.first->close();
                        delete p_streams.second.first;
                    }
                    throw std::runtime_error("file did not open");
                }
                a->second.second = 1;
                a->first = pair.first; //что тебе не нравится??
            }
        }
    }
}

client_logger::client_logger(
    client_logger const &other)
{
    copy(other);
}

client_logger &client_logger::operator=(
    client_logger const &other){
    if (this != &other)
    {
        clear();
        copy(other);
    }
    return *this;
}

client_logger::client_logger(
    client_logger &&other) noexcept
{
    move(std::move(other));
}

client_logger &client_logger::operator=(
    client_logger &&other) noexcept {
    if (this != &other)
    {
        clear();
        move(std::move(other));
    }
    return *this;

}

client_logger::~client_logger() noexcept
{
    for (auto const &pair: file_path_severity){
        if (pair.first != "cerr")
            map_streams[pair.first].second -= 1;
            if (map_streams[pair.first].second == 0){
                map_streams[pair.first].first -> close();
                delete map_streams[pair.first].first;
                map_streams.erase(pair.first);
            }
    }
}

logger const *client_logger::log(
    const std::string &text,
    logger::severity severity) const noexcept
{
    throw not_implemented("logger const *client_logger::log(const std::string &text, logger::severity severity) const noexcept", "your code should be here...");
}

void client_logger::copy(const client_logger & other){
    file_path_severity = other.file_path_severity;
    format_str = other.format_str;
}

void client_logger::move(client_logger && other){
    file_path_severity = std::move(other.file_path_severity);
    format_str = std::move(other.format_str);
}

void client_logger::clear() { // чтооо с тобой не так
    file_path_severity.clear();
    format_str = " ";
}