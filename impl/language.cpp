#include "parser.h"
#include "lexer.h"

#include "ast.h"
#include "definitions.h"

#include <fstream>
#include <iostream>
#include <memory>
#include <variant>

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

    auto name = static_p(NAME).transform<std::string>([](auto tree) { return tree.value; });

    auto arg = name.transform<std::shared_ptr<ast_arg>>([](auto tree) {
        return std::make_shared<ast_arg>(tree);
    });

    auto term = lazy<std::shared_ptr<ast_term>>();
    auto expression = lazy<std::shared_ptr<ast_expression>>();

    auto mul = (term & static_p(MUL) & term).construct<ast_mul, 0, 2>();
    auto div = (term & static_p(DIV) & term).construct<ast_div, 0, 2>();

    auto div_mul = (mul | div).variant_upcast<ast_term>();

    auto add = (div_mul & static_p(PLUS)  & expression).construct<ast_add, 0, 2>();
    auto sub = (div_mul & static_p(MINUS) & expression).construct<ast_sub, 0, 2>();

    auto&& expression_definition = (add | sub | term)
        .variant_upcast<ast_expression>();

    expression.set(expression_definition).own(add, sub, mul, div);

    auto body = lazy<std::shared_ptr<ast_body>>();

    auto less = (expression & static_p(LESS) & expression).construct<ast_less, 0, 2>();
    auto less_or_equal = (expression & static_p(LESS_OR_EQUAL) & expression)
        .construct<ast_less_or_equal, 0, 2>();

    auto greater = (expression & static_p(GREATER) & expression).construct<ast_greater, 0, 2>();
    auto greater_or_equal = (expression & static_p(GREATER_OR_EQUAL) & expression)
        .construct<ast_greater_or_equal, 0, 2>();

    auto equals = (expression & static_p(EQUALS) & expression).construct<ast_less, 0, 2>();
    auto not_equals = (expression & static_p(NOT_EQUAL) & expression).construct<ast_less, 0, 2>();

    auto cond = (less | less_or_equal | greater | greater_or_equal | equals | not_equals)
        .variant_upcast<ast_cond>()
        .own(less, less_or_equal, greater, greater_or_equal, equals, not_equals);


    auto if_p = (static_p(IF) & static_p(LRB) & cond & static_p(RRB) & body)
        .construct<ast_if, 2, 4>();

    auto while_p = (static_p(WHILE) & static_p(LRB) & cond & static_p(RRB) & body)
        .construct<ast_while, 2, 4>();

    auto assignment_p = (static_p(LET) & arg & static_p(EQUAL) & expression)
        .construct<ast_assignment, 1, 3>();

    auto reassignment_p = (name & static_p(EQUAL) & expression)
        .construct<ast_reassignment, 0, 2>();

    auto unary_minus = (static_p(MINUS) & term).construct<ast_unary_minus, 1>();

    auto function_call = (name & static_p(LRB) & optional(expression
                                    & many(static_p(COMMA) & expression)) & static_p(RRB))
        .transform<std::shared_ptr<ast_function_call>>([](auto tree) {
            std::string name = std::get<0>(tree);

            std::vector<std::shared_ptr<ast_expression>> args;
            auto& tree_args = std::get<2>(tree);
            if (tree_args) {
                args.push_back(std::move(std::get<0>(*tree_args)));

                for (auto&& left_args: std::get<1>(*tree_args))
                    args.push_back(std::move(std::get<1>(left_args)));
            }

            return std::make_shared<ast_function_call>(name, std::move(args));
        });

    auto number = static_p(NUMBER)
        .transform<std::shared_ptr<ast_number>>([](auto tree) {
            return std::make_shared<ast_number>(std::stoi(tree.value));
        });

    auto wrapped_expression = (static_p(LRB) & expression & static_p(RRB))
        .construct<ast_wrapped_expression, 1>();


    auto&& term_definition = (number | function_call | wrapped_expression | unary_minus)
        .variant_upcast<ast_term>();

    term.set(std::move(term_definition)).own(number, function_call, wrapped_expression, unary_minus);


    auto for_p = (static_p(FOR) & static_p(LRB) & name
                  & static_p(IN) & term & static_p(ELLIPSIS) & term & static_p(RRB) & body)
        .construct<ast_for, 2, 4, 6, 8>();

    auto return_p = (static_p(RETURN) & expression).construct<ast_return, 1>();


    auto statement = ((if_p | while_p | assignment_p | reassignment_p | for_p | return_p) & static_p(SEMICOLON))
        .transform<std::shared_ptr<ast_statement>>([](auto tree) {
            return std::visit([](auto&& alternative) {
                return std::static_pointer_cast<ast_statement>(alternative);
            }, std::get<0>(tree));
        }).own(if_p, while_p, assignment_p, reassignment_p, for_p, return_p);


    auto&& body_definition = (static_p(LCB) & many(statement) & static_p(RCB)).construct<ast_body, 1>();

    body.set(std::move(body_definition));


    auto function = (static_p(DEFUN) & name & static_p(LRB)
                     & optional(arg
                                & many(static_p(COMMA) & arg)) & static_p(RRB) & body)
        .transform<std::shared_ptr<ast_function>>([](auto tree) {
            std::string name = std::get<1>(tree);

            std::vector<std::shared_ptr<ast_arg>> args;
            auto& tree_args = std::get<3>(tree);
            if (tree_args) {
                args.push_back(std::move(std::get<0>(*tree_args)));

                for (auto&& left_args: std::get<1>(*tree_args))
                    args.push_back(std::move(std::get<1>(left_args)));
            }

            return std::make_shared<ast_function>(name, std::move(args), std::move(std::get<5>(tree)));
        });


    auto program = many(function)
        .transform<std::shared_ptr<ast_program>>([](auto tree) {
            return std::make_shared<ast_program>(tree);
        });

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
    for (auto el: lexems)
        std::cout << el.location.underlined_location(&program_str);

    lexems.push_back(lang::END_LEXEM);

    // auto parser = create_program_parser();

    auto lexem_iterator = lexems.begin();
    // if ((*program.parse(lexem_iterator))->m_functions.size() > 0)
    //     std::cout << "works?!" << std::endl;

    auto parsed = program.parse(lexem_iterator);
    if (parsed)
        (*parsed)->show();


    // return program.own(function, body, statement, expression, term, arg, cond);
}



int main(void) {
    create_program_parser();
}
