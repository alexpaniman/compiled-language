#include "graphviz.h"

#include "test-framework.h"
#include "dfs-visualizer.h"
#include "lexer.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <vector>

#define INSERT_CONNECTIONS(from, to, ...)                                    \
    do {                                                                     \
        char __keys_content[] = { __VA_ARGS__ };                             \
        size_t __size =  sizeof(__keys_content) / sizeof (*__keys_content);  \
                                                                             \
        for (size_t i = 0; i < __size; ++ i)                                 \
            hash_table_insert(&(from)->transition, __keys_content[i], (to)); \
    } while (false)

#define CREATE_STATE(name )                                                  \
    trie *name = NULL; /* Can't wrap it in do/while, variable declaration */ \
    TRY trie_create(&name) ASSERT_SUCCESS();                                 \

void render_graph_and_print_file(trie* root, const std::map<generic_token_t, std::string>& token_names = {}, trie* current = NULL) {
    digraph graph = trie_vis_create_graph(root, token_names);
    digraph_render_and_destory(&graph);
}


enum {
    ARROW, COLON, COMMA, DEFUN, DIV, ELLIPSIS, ELSE, EQUAL, EQUALS,
    FOR, GREATER, GREATER_OR_EQUAL, IF, IN, INT, LESS, LESS_OR_EQUAL,
    LET, LRB, MINUS, MUL, NAME, NOT_EQUAL, NUMBER, PLUS, RETURN,
    RRB, SEMICOLON, WHILE, LCB, RCB
};

static void print_all_lexems(lang::lexer& lexer, std::string& program, std::vector<lang::lexem>& lexems) {
    for (const auto& lexem: lexems) {
        std::cout << "Recognised token: <" << lexer.get_token_name(lexem.id) << ">"
                  << ": '" << lexem.value << "'" <<  std::endl;

        std::cout << lexem.location.underlined_location(&program) << std::endl;
    }
}

TEST(few_rules) {
    lang::lexer lexer;

    lexer.ignore_rule("[\n \t]([\n \t])");

    lexer.add_rule(named(FOR),   "for");
    lexer.add_rule(named(ARROW), "a(a)a(a)([abc]m)(aba)");

    std::string program = "aaaaabmcm   for\nfor";

    auto lexems = lexer.analyse(program);
    print_all_lexems(lexer, program, lexems);
}

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

TEST(full_language) {
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

    std::string file_name = "res/test.prog";
    std::string program = read_whole_file(file_name);

    auto lexems = lexer.analyse(program, file_name);
    print_all_lexems(lexer, program, lexems);
}


int main(void) {
    return test_framework_run_all_unit_tests();
}
