#include "graphviz.h"
#include "parser.h"
#include "lexer.h"

#include "ast.h"
#include "definitions.h"

#include <fstream>
#include <iostream>
#include <memory>
#include <variant>
#include <chrono>

static std::string read_whole_file(std::string file_name) {
    std::ifstream file(file_name);

    std::string line;
    std::string file_contents;

    while (std::getline(file, line)) {
        file_contents += line;
        file_contents.push_back('\n');
    }  

    return file_contents;
}

void create_program_parser() {
    using namespace lang;
    using enum language_lexem;

    // ----------------------------------------- PRIMITIVES ----------------------------------------
    auto name                  = transform(static_p(NAME), [](auto tree) { return tree.value; });
    alloc_p<ast_number> number = 
        transform(static_p(NUMBER), [](auto tree) { return std::stoi(tree.value); });

    // ========================================= ARITHMETIC ========================================

    // -------------------------------------------- BASIC ------------------------------------------

    lazy_w<std::shared_ptr<ast_term>> factor;
    lazy_w<std::shared_ptr<ast_term>> term;
    lazy_w<std::shared_ptr<ast_expression>> expression;

    // --------------------------------------- 1ST PRECEDENCE --------------------------------------
    alloc_p<ast_var> var = name;

    alloc_p<ast_mul> mul = factor & ignore_p(MUL) & term;
    alloc_p<ast_div> div = factor & ignore_p(DIV) & term;

    term = variant_upcast<ast_term>(mul | div | factor);

    // --------------------------------------- 2ND PRECEDENCE --------------------------------------
    alloc_p<ast_add> add = term & ignore_p(PLUS)  & expression;
    alloc_p<ast_sub> sub = term & ignore_p(MINUS) & expression;

    expression = variant_upcast<ast_expression>(add | sub | term);

    // ----------------------------------------- COMPARISON ----------------------------------------
    auto named_comparison = [&](named_lexem lexem) { return expression & ignore_parser(lexem) & expression; };
    #define comparison(id) named_comparison(named_lexem { id, #id })

    alloc_p<ast_less>             less             = comparison(LESS);
    alloc_p<ast_less_or_equal>    less_or_equal    = comparison(LESS_OR_EQUAL);
    alloc_p<ast_greater>          greater          = comparison(GREATER);
    alloc_p<ast_greater_or_equal> greater_or_equal = comparison(GREATER_OR_EQUAL);
    alloc_p<ast_equals>           equals           = comparison(EQUALS);
    alloc_p<ast_not_equals>       not_equal        = comparison(NOT_EQUAL);

    #undef comparison

    auto cond = variant_upcast<ast_cond>(less | less_or_equal | greater | greater_or_equal | equals | not_equal);

    // ---------------------------------------- ASSIGNMENT -----------------------------------------
    auto assignment = name & ignore_p(EQUAL) & expression;

    alloc_p<ast_assignment>   assignment_p   = ignore_p(LET) & assignment;
    alloc_p<ast_reassignment> reassignment_p = assignment;

    // ------------------------------------------ TERMS --------------------------------------------
    alloc_p<ast_unary_minus> unary_minus = ignore_p(MINUS) & factor;

    auto arguments = ignore_p(LRB) & separated_by(expression, ignore_p(COMMA)) & ignore_p(RRB);
    alloc_p<ast_function_call> function_call = name & arguments;

    alloc_p<ast_wrapped_expression> wrapped_expression = ignore_p(LRB) & expression & ignore_p(RRB);

    // <== Term declaration (see forward declaration in "arithmetic" section)
    factor = variant_upcast<ast_term>(wrapped_expression | function_call | number | var); // TODO: unary minus

    // ========================================= STATMENTS =========================================

    lazy_w<std::shared_ptr<ast_body>> body; // Forward declared (recursive declaration)

    // ---------------------------------------- CONDITIONAL ----------------------------------------
    auto condition_and_body = ignore_p(LRB) & cond & ignore_p(RRB) & body;

    alloc_p<ast_if> if_p = ignore_p(IF) & condition_and_body;
    alloc_p<ast_while> while_p = ignore_p(WHILE) & condition_and_body;

    // ---------------------------------------------------------------------------------------------
    alloc_p<ast_for> for_p = ignore_p(FOR) & ignore_p(LRB) & name &
        ignore_p(IN) & factor & ignore_p(ELLIPSIS) & factor & ignore_p(RRB) & body;

    alloc_p<ast_return> return_p = ignore_p(RETURN) & expression;

    // ---------------------------------------------------------------------------------------------
    auto statement_without_semicolon = variant_upcast<ast_statement>(if_p | while_p | for_p);
    auto statement_with_semicolon    = variant_upcast<ast_statement>(
        assignment_p | reassignment_p | return_p & ignore_p(SEMICOLON));

    auto statement = variant_upcast<ast_statement>(statement_with_semicolon | statement_without_semicolon);

    // <== Body declaration (see forward declaration in "statements" section)
    body = construct<ast_body>(ignore_p(LCB) & many(statement) & ignore_p(RCB));

    // ---------------------------------------- TOP LEVEL ------------------------------------------
    auto argument_declaration = ignore_p(LRB) & separated_by(name, ignore_p(COMMA)) & ignore_p(RRB);
    alloc_p<ast_function> function = ignore_p(DEFUN) & name & argument_declaration & body;

    auto program = construct<ast_program>(many(function)); // <== Topmost parser
    // ---------------------------------------------------------------------------------------------

    std::string file_name = "res/test.prog";
    std::string program_str = read_whole_file(file_name);

    lang::lexer lexer;

    // Whitespace rule
    lexer.ignore_rule("[\n \t]([\n \t])");

    lexer.add_rules({
        { named(ARROW),            "->"                        },
        { named(COLON),            ":"                         },
        { named(COMMA),            ","                         },
        { named(ELLIPSIS),         ".."                        },

        { named(EQUAL),            "="                         },

        { named(EQUALS),           "=="                        },
        { named(NOT_EQUAL),        "!="                        },
        { named(GREATER),          ">"                         },
        { named(GREATER_OR_EQUAL), ">="                        },
        { named(LESS),             "<"                         },
        { named(LESS_OR_EQUAL),    "<="                        },

        { named(MINUS),            "-"                         },
        { named(MUL),              "*"                         },
        { named(PLUS),             "+"                         },
        { named(DIV),              "/"                         },
        { named(SEMICOLON),        ";"                         },

        { named(LCB),              "{"                         },
        { named(RCB),              "}"                         },

        { named(LRB),              "[(]"                       },
        { named(RRB),              "[)]"                       },

        { named(DEFUN),            "defun"                     },
        { named(RETURN),           "return"                    },

        { named(IF),               "if"                        },
        { named(ELSE),             "else"                      },

        { named(LET),              "let"                       },

        { named(WHILE),            "while"                     },

        { named(FOR),              "for"                       },
        { named(IN),               "in"                        },

        { named(INT),              "int"                       },

        { named(NAME),             "[A-Za-z_]([A-Za-z0-9_])"   },
        { named(NUMBER),           "[0-9]([0-9])"              }
    });

    std::vector<lang::lexem> lexems = lexer.analyse(program_str, file_name);
    for (auto el: lexems) {
        std::cout << "note: detected lexem: <"<< lexer.get_token_name(el.id) << ">\n";
        std::cout << el.location.underlined_location(&program_str) << "\n\n";
    }

    lexems.push_back(lang::END_LEXEM);

    auto start = std::chrono::high_resolution_clock::now();
    auto lexem_iterator = lexems.begin();
    auto finish = std::chrono::high_resolution_clock::now();
    std::cout << "lexical analysis: " << (double) std::chrono::duration_cast<std::chrono::nanoseconds>(finish-start).count() / 1e9 << "s\n";

    auto show_graph = [](auto&& graph) { digraph_render_and_destory(&graph); };
    show_graph(program.graph());

    start = std::chrono::high_resolution_clock::now();

    auto parsed = program.parse(lexem_iterator);

    finish = std::chrono::high_resolution_clock::now();
    std::cout << "         parsing: " << (double) std::chrono::duration_cast<std::chrono::nanoseconds>(finish-start).count() / 1e9 << "s\n";

    if (parsed)
        (*parsed)->show();


    // return program.own(function, body, statement, expression, term, arg, cond);
}



int main(void) {
    create_program_parser();
}
