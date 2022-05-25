#include "parser.h"
#include "aho.h"
#include "lexer.h"
#include <optional>

namespace lang {

    lexem_parser_p::lexem_parser_p(language_lexem id): m_token_id(id) {}

    std::optional<lang::lexem> lexem_parser_p::parse(lexem_iterator& lexems) {
        // EMPTY_TOKEN_ID is used as EOF marker
        if (lexems->id == language_lexem::END)
            return std::nullopt;

        if (lexems->id == m_token_id) {
            lang::lexem current = *lexems;

            ++ lexems; // Advance to the next token
            return current;
        }

        return std::nullopt;
    }

    parser_w<lexem> static_p(language_lexem id) {
        auto&& new_parser = std::make_shared<lexem_parser_p>(id);
        return parser_w(*new_parser).own(new_parser);
    }

}
