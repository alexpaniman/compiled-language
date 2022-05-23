#include "lexer.h"

#include "aho.h"
#include "ansi-colors.h"

#include "graphviz.h"
#include "dfs-visualizer.h"

#include <iostream>
#include <sstream>
#include <fstream>
#include <numeric>
#include <iomanip>
#include <stdexcept>
#include <string>

namespace lang {

    static std::istream& goto_line(std::istream& file, std::size_t num){
        file.seekg(std::ios::beg);
        for(int i = 0; i < num - 1; ++ i){
            file.ignore(std::numeric_limits<std::streamsize>::max(),'\n');
        }

        return file;
    }

    continuous_location::continuous_location(std::string _file_name, int _length, position_in_file _position)
        : file_name(_file_name), length(_length), position(_position) {};

    std::string continuous_location::underlined_location(std::string* source) const {
        std::stringstream ss;

        if (file_name.empty() && source == nullptr) {
            ss << "In " << file_name << ":" << position.line << ":" << position.column << "\n";
            return ss.str();
        }

        std::string line;
        if (source == nullptr) { // Use source if provided
            std::fstream file(file_name);
            goto_line(file, position.line);

            std::getline(file, line);
        } else {
            std::istringstream input_string(*source);
            goto_line(input_string, position.line);

            std::getline(input_string, line);
        }

        const size_t line_number_alignment = 6; // 6 is value used by GCC

        // Print file and line description
        ss << "In " << COLOR_BOLD;

        if (!file_name.empty())
            ss << file_name;
        else
            ss << "[define-inline]";

        ss << ":" << position.line << ":" << position.column << COLOR_RESET << ":" << "\n";

        // Prepare to print current line (print line number like GCC does)
        ss << std::setw(line_number_alignment) << position.line << " |";

        // Print line up to desired location
        for (int i = 0; i < position.column - 1; ++ i) 
            ss << line[i];

        // Switch color to highlight location
        ss << COLOR_BLUE;

        // Print actual content linked with location
        for (int i = position.column - 1, j = 0; j < length; ++ j, ++ i) 
            ss << line[i];

        // Reset printing color for the rest of the line
        ss << COLOR_RESET;

        // Print rest of the line
        for (int i = position.column + length - 1; i < line.size(); ++ i) 
            ss << line[i];

        ss << "\n";

        // GCC like style, print space before | at the same level as before 
        for (int i = 0; i < line_number_alignment; ++ i)
            ss << ' ';

        ss << " |";

        // Append spaces up to desired location
        for (int i = 0; i < position.column - 1; ++ i)
            ss << ' ';

        // Underline location
        ss << COLOR_BLUE << "^";

        for (int i = 0; i < length - 1; ++ i)
            ss << "~";

        ss << COLOR_RESET << "\n";

        return ss.str();
    }

    std::ostream& operator<<(std::ostream& os, const continuous_location& location) {
        os << location.underlined_location();
        return os;
    }


    lexem::lexem(int _id, std::string _value, continuous_location _location)
        : id(_id), value(_value), location(_location) {};

    std::ostream &operator<<(std::ostream &os, const lexem &lexem) {
        os << lexem.location;
        return os;
    }      


    lexer::lexer(): m_lexer_nfsm(new raw_trie()), m_compiled_lexer(nullptr) {}

    void lexer::ignore_rule(std::string ignore) {
        regex_parse(m_lexer_nfsm, ignore.c_str(), IGNORED_TOKEN_ID);
        m_rule_order.push_back(IGNORED_TOKEN_ID);
    }

    void lexer::add_rule(named_lexem new_lexem, std::string regex) {
        regex_parse(m_lexer_nfsm, regex.c_str(), new_lexem.id);

        m_rule_order.push_back(new_lexem.id);
        m_token_names[new_lexem.id] = new_lexem.name;
    }

    void lexer::add_rules(std::initializer_list<std::pair<named_lexem, std::string>> rules) {
        for (auto &[new_lexem, regex]: rules)
            add_rule(new_lexem, regex);
    }

    std::string lexer::get_token_name(generic_token_t id) {
        return m_token_names.at(id);
    }      

    void lexer::compile() {
        std::set<trie*> tries = {};
        std::set<raw_trie*> raw_tries = {};

        trie_nfsm_to_dfsm(m_lexer_nfsm, &m_compiled_lexer, &tries, &raw_tries, m_rule_order);
    }

    digraph lexer::draw_graph(trie* current) {
        if (m_compiled_lexer == nullptr)
            compile();

        return trie_vis_create_graph(m_compiled_lexer, m_token_names, current);
    }

    void lexer::show_graph(trie* current) {
        digraph graph = draw_graph(current);
        digraph_render_and_destory(&graph);
    }      

    struct captured_token {
        generic_token_t id;
        position_in_file last_pos;
    };

    std::vector<lexem> lexer::analyse(std::string program, std::string file_name) {
        if (m_compiled_lexer == nullptr)
            compile();

        std::vector<lexem> lexems;

        // Current position in /program/, will be incremented on first iteration:
        position_in_file pos { .point = 0, .line = 1, .column = 0 };

        // Begining of last token, and last met token, begining of text at first:
        position_in_file beg { .point = 1, .line = 1, .column = 1 };

        trie* current_state = m_compiled_lexer;
        // <= for pseudo after_the_last state to emit last token
        for (int i = 0; i <= program.size(); ++ i) {
            position_in_file last_pos = pos;

            char symbol = '\0';
            if (i != program.size()) // For after the last state
                symbol = program[i];

            pos.point = i + 1;

            if (i == 0 || program[i - 1] != '\n')
                ++ pos.column;
            else {
                ++ pos.line;
                pos.column = 1;
            }

            if (!current_state->transition.contains(symbol)) {
                int length = pos.point - beg.point;

                continuous_location location(file_name, length, beg);
                std::string current_token(&program[beg.point - 1], length);

                // Test if analysis failed
                if (current_state->token == EMPTY_TOKEN_ID)
                    throw std::runtime_error("error: couldn't recognise token:\n" +
                                             location.underlined_location(&program));
                // Emit current lexem
                if (current_state->token != IGNORED_TOKEN_ID)
                    lexems.push_back(lexem(current_state->token, current_token, location));

                if (i == program.size()) // Current iteration is after the last
                    break;

                beg = pos; // Register start of a new token
                pos = last_pos; -- i; // Rewind one symbol back

                // And process it again from lexer's begining:
                current_state = m_compiled_lexer;
                continue;
            }

            current_state = current_state->transition[symbol];
        }

        return lexems;
    }

};
