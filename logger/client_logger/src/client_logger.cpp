//#include <not_implemented.h>
#include <iostream>
#include <fstream>

#include "../include/client_logger.h"

std::map <std::string, std::pair<std::ofstream*, int>> client_logger::map_streams = std::map <std::string, std::pair<std::ofstream*, int>>(); // инициализация статического поля

client_logger::client_logger(std::map <std::string, unsigned char> const & path_severity, std::string const & format):file_path_severity(path_severity){
    format.empty()? format_str = "%t %d %m %s": format_str = format;

//    for (auto i=0; i < format.size();  i++)
//    {
//        if (format[i] == '%')
//        {
//            if (i == format.size()-1)
//            {
//                throw std::logic_error("Format is invalid");
//            }
//            if (!isalpha(format[i+1])){
//                throw std::logic_error("Format is invalid");
//            }
//            format_str+=format[i];
//        }
//    }

    for (auto const &pair: path_severity)
    {
        if (pair.first != "cerr")
        {
            // std::pair<std::string, std::pair<std::ofstream*, int>> a
            auto a = map_streams.find(pair.first);

            if (a != map_streams.end())
            {
                a->second.second+=1;
            }
            else
            {
                std::ofstream* file = new std::ofstream(pair.first);
                if (!file->is_open()) { // если текущий файл не открылся
                    delete file;

                    // проходим по мапе path_severity от начала до текущего, чтобы закрыть ранее открытые файлы
                    auto iter = path_severity.find(pair.first);
                    for (auto i = path_severity.begin(); i != iter; ++i) {
                        if (i -> first != "cerr")
                        {
                            map_streams[i -> first].second -= 1;
                            if (map_streams[i -> first].second == 0)
                            {
                                map_streams[i -> first].first -> flush();
                                map_streams[i -> first].first -> close();
                                delete map_streams[i -> first].first;
                                map_streams.erase(i -> first);
                            }
                        }
                    }
                    throw std::runtime_error("file did not open");
                }
                // если всё ок, то кладём в мапу
                map_streams.insert(std::make_pair(pair.first, std::make_pair(file, 1)));
            }
        }
    }
}

client_logger::client_logger(
    client_logger const &other)
{
    copy(other);

    for (auto& itr : file_path_severity)
    {
        auto pair = map_streams.find(itr.first);
        pair -> second.second += 1;
    }
}

client_logger &client_logger::operator=(
    client_logger const &other){
    if (this != &other)
    {
        for (const auto& itr : file_path_severity)
        {
            auto& pair = map_streams[itr.first];
            pair.second -= 1;

            if (pair.second == 0)
            {
                pair.first -> close();
                delete pair.first;
            }
        }

        clear();
        copy(other);

        for (const auto& local_map : file_path_severity)
        {
            auto& pair = map_streams[local_map.first];
            pair.second += 1;
        }

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
    if (this != &other) {
        for (const auto &iter: file_path_severity) {
            auto &pair = map_streams[iter.first];
            pair.second -= 1;

            if (pair.second == 0) {
                pair.first->close();
                delete pair.first;
            }
        }

        clear();
        move(std::move(other));
    }

    return *this;
}

client_logger::~client_logger() noexcept
{
    for (auto const &pair: file_path_severity){
        if (pair.first != "cerr")
        {
            map_streams[pair.first].second -= 1;
            if (map_streams[pair.first].second == 0){
                map_streams[pair.first].first -> flush();
                map_streams[pair.first].first -> close();
                delete map_streams[pair.first].first;
                map_streams.erase(pair.first);
            }
        }
    }
}

logger const *client_logger::log(
    const std::string &text,
    logger::severity severity) const noexcept
{
    std::string str = formating_string(text, severity);

    for (auto const &iter: file_path_severity)
    {
        if ((iter.second & (1 << static_cast<int>(severity))) != 0)
        {
            if (iter.first != "cerr")
                *(map_streams[iter.first].first) << str << std::endl;
            else
                std::cerr << str << std::endl;
        }
    }
    return this;
}

//std::string client_logger::formating_string(std::string const &text, logger::severity severity) const
//{
//    std::string str;
//
//    for(auto i = 0; i < format_str.size(); i++) {
//        if(format_str[i] == '%' && (i + 1) != format_str.size()) {
//            switch(format_str[i+1]) {
//                case 'd':
//                    str += current_datetime_to_string().substr(0, 10);
//                    i++;
//                    break;
//                case 't':
//                    str += current_datetime_to_string().substr(10, 9);
//                    i++;
//                    break;
//                case 's':
//                    str += severity_to_string(severity);
//                    i++;
//                    break;
//                case 'm':
//                    str += text;
//                    i++;
//                    break;
//                default:
//                    str += '%';
//                    str += format_str[i+1];
//                    i++;
//                    break;
//            }
//        }
//        str += format_str[i];
//    }
//
//    return str;
//}

std::string client_logger::formating_string(std::string const &message, logger::severity severity) const {
    std::string res;
    auto ptr = format_str.begin();
    while(ptr != format_str.end()) {
        if(*ptr == '%') {
            ++ptr;
            if(ptr == format_str.end()) throw std::logic_error("Format error!");
            switch (*ptr) {
                case 'd':
                    res += current_datetime_to_string().substr(0, 10);
                    break;
                case 't':
                    res += current_datetime_to_string().substr(10, 9);
                    break;
                case 's':
                    res += logger::severity_to_string(severity);
                    break;
                case 'm':
                    res += message;
                    break;
                default:
                    throw std::logic_error("Unknown modificator");
                    break;
            }
        }else res.push_back(*ptr);
        ++ptr;
    }
    return res;
}

void client_logger::copy(const client_logger & other){
    file_path_severity = other.file_path_severity;
    format_str = other.format_str;
}

void client_logger::move(client_logger && other){
    file_path_severity = std::move(other.file_path_severity);
    format_str = std::move(other.format_str);
}

void client_logger::clear() {
    file_path_severity.clear();
    format_str = " ";
}
