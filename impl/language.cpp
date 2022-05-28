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
    auto name   = transform(static_p(NAME), [](auto tree) { return tree.value; });

    auto number = construct<ast_number>(
        transform(static_p(NUMBER), [](auto tree) { return std::stoi(tree.value); })
    );

    // ========================================= ARITHMETIC ========================================

    // -------------------------------------------- BASIC ------------------------------------------
    auto factor = lazy<std::shared_ptr<ast_term>>();           // Forward declared (recursive declaration)
    auto term   = lazy<std::shared_ptr<ast_term>>();           // Forward declared (recursive declaration)
    auto expression = lazy<std::shared_ptr<ast_expression>>(); // Forward declared (recursive declaration)

    // --------------------------------------- 1ST PRECEDENCE --------------------------------------
    auto var = construct<ast_var>(name);

    auto mul = construct<ast_mul>(factor & ignore_p(MUL) & term);
    auto div = construct<ast_div>(factor & ignore_p(DIV) & term);
    term.set(variant_upcast<ast_term>(mul | div | factor));

    // --------------------------------------- 2ND PRECEDENCE --------------------------------------
    auto add = construct<ast_add>(term & ignore_p(PLUS)  & expression);
    auto sub = construct<ast_sub>(term & ignore_p(MINUS) & expression);

    expression.set(variant_upcast<ast_expression>(add | sub | term));

    // ----------------------------------------- COMPARISON ----------------------------------------
    auto named_comparison = [&](named_lexem lexem) { return expression & ignore_parser(lexem) & expression; };
    #define comparison(id) named_comparison(named_lexem { id, #id })

    auto less             = construct<ast_less            >(comparison(LESS)            );
    auto less_or_equal    = construct<ast_less_or_equal   >(comparison(LESS_OR_EQUAL)   );
    auto greater          = construct<ast_greater         >(comparison(GREATER)         );
    auto greater_or_equal = construct<ast_greater_or_equal>(comparison(GREATER_OR_EQUAL));
    auto equals           = construct<ast_equals          >(comparison(EQUALS)          );
    auto not_equal        = construct<ast_not_equals      >(comparison(NOT_EQUAL)       );

    #undef comparison

    auto cond = variant_upcast<ast_cond>(less | less_or_equal | greater | greater_or_equal | equals | not_equal);

    // ---------------------------------------- ASSIGNMENT -----------------------------------------
    auto assignment = name & ignore_p(EQUAL) & expression;

    auto assignment_p   = construct<ast_assignment  >(ignore_p(LET) & assignment);
    auto reassignment_p = construct<ast_reassignment>(                assignment);

    // ------------------------------------------ TERMS --------------------------------------------
    auto unary_minus = construct<ast_unary_minus>(ignore_p(MINUS) & factor);

    auto arguments = ignore_p(LRB) & separated_by(expression, ignore_p(COMMA)) & ignore_p(RRB);
    auto function_call = construct<ast_function_call>(name & arguments);

    auto wrapped_expression =
        construct<ast_wrapped_expression>(ignore_p(LRB) & expression & ignore_p(RRB));

    // <== Term declaration (see forward declaration in "arithmetic" section)
    factor.set(variant_upcast<ast_term>(wrapped_expression | function_call | number | var)); // TODO: unary minus

    // ========================================= STATMENTS =========================================

    auto body = lazy<std::shared_ptr<ast_body>>(); // Forward declared (recursive declaration)

    // ---------------------------------------- CONDITIONAL ----------------------------------------
    auto condition_and_body = ignore_p(LRB) & cond & ignore_p(RRB) & body;

    auto if_p    = construct<ast_if   >(ignore_p(IF)    & condition_and_body);
    auto while_p = construct<ast_while>(ignore_p(WHILE) & condition_and_body);

    // ---------------------------------------------------------------------------------------------
    auto for_p = construct<ast_for>(ignore_p(FOR) & ignore_p(LRB) & name
                & ignore_p(IN) & factor & ignore_p(ELLIPSIS) & factor & ignore_p(RRB) & body);

    auto return_p = construct<ast_return>(ignore_p(RETURN) & expression);

    // ---------------------------------------------------------------------------------------------
    auto statement_without_semicolon = variant_upcast<ast_statement>(if_p | while_p | for_p);
    auto statement_with_semicolon    = variant_upcast<ast_statement>(
        assignment_p | reassignment_p | return_p & ignore_p(SEMICOLON));

    auto statement = variant_upcast<ast_statement>(statement_with_semicolon | statement_without_semicolon);

    // <== Body declaration (see forward declaration in "statements" section)
    body.set(construct<ast_body>(ignore_p(LCB) & many(statement) & ignore_p(RCB)));

    // ---------------------------------------- TOP LEVEL ------------------------------------------
    auto argument_declaration = ignore_p(LRB) & separated_by(name, ignore_p(COMMA)) & ignore_p(RRB);
    auto function = construct<ast_function>(ignore_p(DEFUN) & name & argument_declaration & body);

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
