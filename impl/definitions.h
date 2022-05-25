#pragma once

enum class language_lexem {
    ARROW, COLON, COMMA, ELLIPSIS,

    EQUAL,

    EQUALS, NOT_EQUAL, GREATER, GREATER_OR_EQUAL, LESS, LESS_OR_EQUAL,

    MINUS, MUL, PLUS, DIV, SEMICOLON,

    LCB, RCB,

    LRB, RRB,

    DEFUN, RETURN,

    IF, ELSE,

    LET,

    WHILE,

    FOR, IN,

    INT,

    NAME, NUMBER,

    END // Special mark used by parser
};
