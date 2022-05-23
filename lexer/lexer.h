#pragma once

#include "aho.h"
#include "graphviz.h"
#include <cstddef>
#include <initializer_list>
#include <string_view>
#include <string>
#include <vector>

namespace lang {

    struct position_in_file {
        int point;
        int line, column;
    };

    class continuous_location final {
    public:
        const std::string file_name;
        const int length;

        const position_in_file position;

        continuous_location(std::string file_name, int length, position_in_file position);

        std::string underlined_location(std::string* source = nullptr) const;
    };

    std::ostream& operator<<(std::ostream& os, const continuous_location& location);


    class lexem final { 
    public:
        const continuous_location location;

        const int id;
        const std::string value;

        lexem(int new_id, std::string value, continuous_location location);
    };

    std::ostream& operator<<(std::ostream& os, const lexem& lexem);


    const generic_token_t EMPTY_TOKEN_ID   = -1;
    const generic_token_t IGNORED_TOKEN_ID = -2;

    struct named_lexem {
        generic_token_t id;
        std::string name;
    };
    
    class lexer final {
    public:
        lexer();

        void ignore_rule(std::string ignore_regex);

        void add_rule(named_lexem new_lexem, std::string regex);
        void add_rules(std::initializer_list<std::pair<named_lexem, std::string>> rules);

        std::string get_token_name(generic_token_t id);

        digraph draw_graph(trie* current = nullptr);
        void show_graph(trie* current = nullptr);

        void compile();
        std::vector<lexem> analyse(std::string program, std::string file_name = "");

    private:
        raw_trie* m_lexer_nfsm;
        trie* m_compiled_lexer;

        std::map<generic_token_t, std::string> m_token_names;
        std::vector<generic_token_t> m_rule_order;
    };

    #define named(id) lang::named_lexem { id, #id }

}

