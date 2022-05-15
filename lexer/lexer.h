#pragma once

#include <cstddef>
#include <string_view>
#include <string>
#include <vector>

namespace lang::lexer {

    struct position_in_file {
        size_t point;
        size_t line, column;
    };

    class continuous_location {
    public:
        const std::string file_name;
        const size_t length;

        const position_in_file position;

        continuous_location(std::string file_name, size_t length, position_in_file position);

        std::string print_marked_location() const;
    };

    std::ostream& operator<<(std::ostream& os, const continuous_location& location);


    class lexem { 
    public:
        const int id;
        const std::string_view value;

        lexem(int new_id, std::string_view value,
              continuous_location location);

    private:
        const continuous_location location;
    };

    std::ostream& operator<<(std::ostream& os, const lexem& lexem);

    
    std::vector<lexem> lexically_analyse(std::string program);

}

