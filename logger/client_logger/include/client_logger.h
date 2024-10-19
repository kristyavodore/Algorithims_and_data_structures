#ifndef MATH_PRACTICE_AND_OPERATING_SYSTEMS_CLIENT_LOGGER_H
#define MATH_PRACTICE_AND_OPERATING_SYSTEMS_CLIENT_LOGGER_H

#include <C:\Users\Krist\CLionProjects\Algorithims_and_data_structures\logger\logger\include\logger.h>
#include <map>
#include "client_logger_builder.h"

class client_logger final:
    public logger
{

    std::map <std::string, unsigned char> file_path_severity;
    std::string format_str;

    static std::map <std::string, std::pair<std::ofstream*, int>> map_streams;

private:

    client_logger(std::map <std::string, unsigned char> const &file_path_severity, std::string const &format_str);

public:

    client_logger(
        client_logger const &other);

    client_logger &operator=(
        client_logger const &other);

    client_logger(
        client_logger &&other) noexcept;

    client_logger &operator=(
        client_logger &&other) noexcept;

    ~client_logger() noexcept final;

public:

    [[nodiscard]] logger const *log(
        const std::string &message,
        logger::severity severity) const noexcept override;


private:

    void copy (const client_logger & other);
    void move ( client_logger && other);
    void clear();

    std::string formating_string(std::string const &text, logger::severity severity) const;
};

#endif //MATH_PRACTICE_AND_OPERATING_SYSTEMS_CLIENT_LOGGER_H