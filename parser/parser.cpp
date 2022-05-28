#include "parser.h"
#include "graphviz.h"
#include <optional>
#include <string>

namespace lang {

    lexem_parser_p::lexem_parser_p(named_lexem named_token): m_named_token(named_token) {}
        
    void lexem_parser_p::style(node& default_node) {
        default_node.color = GRAPHVIZ_BLUE;
    }

    std::string lexem_parser_p::node_name() {
        return m_named_token.name;
    }

    void lexem_parser_p::connect_children(SUBGRAPH_CONTEXT, std::map<void*, node_id>& graphed, node_id current) {
        return;
    }

    std::optional<lang::lexem> lexem_parser_p::parse(lexem_iterator& lexems) {
        // EMPTY_TOKEN_ID is used as EOF marker
        if (lexems->id == language_lexem::END)
            return std::nullopt;

        if (lexems->id == m_named_token.id) {
            lang::lexem current = *lexems;

            ++ lexems; // Advance to the next token
            return current;
        }

        return std::nullopt;
    }

    parser_w<lexem> static_parser(named_lexem lexem) {
        auto&& new_parser = std::make_shared<lexem_parser_p>(lexem);
        return parser_w(*new_parser).own(new_parser);
    }

    parser_w<ignore> ignore_parser(named_lexem lexem) {
        return ignored(static_parser(lexem));
    }

}
