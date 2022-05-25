#pragma once

#include "graphviz.h"

#include <memory>
#include <vector>
#include <string>

struct ast {
    virtual void show_graph(SUBGRAPH_CONTEXT, node_id parent) = 0;

    digraph graph() {
        return NEW_GRAPH({
            NEW_SUBGRAPH(RANK_NONE, {
                DEFAULT_NODE = {
                    .style = STYLE_BOLD,
                    .color = GRAPHVIZ_BLACK,
                    .shape = SHAPE_CIRCLE,
                };

                DEFAULT_EDGE = {
                    .color = GRAPHVIZ_BLACK,
                    .style = STYLE_SOLID
                };

                node_id root = NODE("root");
                show_graph(CURRENT_SUBGRAPH_CONTEXT, root);
            });
        });
    }

    void show() {
        digraph graph = this->graph();
        digraph_render_and_destory(&graph);
    }
};

struct ast_statement: public ast {};

struct ast_arg: public ast {
    ast_arg(std::string name): m_name(name) {};

    std::string m_name;

    virtual void show_graph(SUBGRAPH_CONTEXT, node_id parent) override {
        EDGE(parent, NODE("%s", m_name.c_str()));
    }
};

struct ast_body: public ast {
    ast_body(std::vector<std::shared_ptr<ast_statement>> statements):
        m_statements(statements) {}

    std::vector<std::shared_ptr<ast_statement>> m_statements;

    virtual void show_graph(SUBGRAPH_CONTEXT, node_id parent) override {
        node_id body = NODE("{ ... }"); EDGE(parent, body);

        for (const auto& statement: m_statements)
            statement->show_graph(CURRENT_SUBGRAPH_CONTEXT, body);
    }
};

struct ast_function: public ast {
    ast_function(std::string name, std::vector<std::shared_ptr<ast_arg>>&& args, std::shared_ptr<ast_body>&& body)
        : m_name(name), m_args(std::move(args)), m_body(std::move(body)) {}

    std::string m_name;

    std::vector<std::shared_ptr<ast_arg>> m_args;
    std::shared_ptr<ast_body> m_body;

    virtual void show_graph(SUBGRAPH_CONTEXT, node_id parent) override {
        node_id function = NODE("defun %s()", m_name.c_str());
        EDGE(parent, function);

        node_id args = NODE("args");
        EDGE(function, args);

        for (const auto& arg: m_args)
            arg->show_graph(CURRENT_SUBGRAPH_CONTEXT, args);

        m_body->show_graph(CURRENT_SUBGRAPH_CONTEXT, function);
    }
};

struct ast_expression: public ast {};

struct ast_term: public ast_expression {};

struct ast_function_call: ast_term {
    ast_function_call(std::string name, std::vector<std::shared_ptr<ast_expression>> parameters)
        : m_name(name), m_parameters(parameters) {}

    std::string m_name;
    std::vector<std::shared_ptr<ast_expression>> m_parameters;

    virtual void show_graph(SUBGRAPH_CONTEXT, node_id parent) override {
        node_id function_call = NODE("%s()", m_name.c_str());
        EDGE(parent, function_call);

        for (const auto& arg: m_parameters)
            arg->show_graph(CURRENT_SUBGRAPH_CONTEXT, function_call);
    }
};

struct ast_unary_minus: public ast_term {
    ast_unary_minus(std::shared_ptr<ast_term> term)
        : m_term(term) {}

    std::shared_ptr<ast_term> m_term;

    virtual void show_graph(SUBGRAPH_CONTEXT, node_id parent) override {
        node_id function = NODE("-");
        EDGE(parent, function);

        m_term->show_graph(CURRENT_SUBGRAPH_CONTEXT, function);
    }
};

struct ast_number: public ast_term {
    ast_number(int number): m_number(number) {}

    int m_number;

    virtual void show_graph(SUBGRAPH_CONTEXT, node_id parent) override {
        node_id number = NODE("%d", m_number);
        EDGE(parent, number);
    }
};

struct ast_var: public ast_term {
    ast_var(std::string name): m_name(name) {}

    std::string m_name;

    virtual void show_graph(SUBGRAPH_CONTEXT, node_id parent) override {
        node_id var = NODE("%d", m_name.c_str());
        EDGE(parent, var);
    }
};

struct ast_wrapped_expression: public ast_term {
    ast_wrapped_expression(std::shared_ptr<ast_expression> expression)
        : m_expression(expression) {}
  
    std::shared_ptr<ast_expression> m_expression;

    virtual void show_graph(SUBGRAPH_CONTEXT, node_id parent) override {
        m_expression->show_graph(CURRENT_SUBGRAPH_CONTEXT, parent);
    }
};

struct ast_mul: public ast_term {
    ast_mul(std::shared_ptr<ast_term> lhs,
            std::shared_ptr<ast_term> rhs)
        : m_expression { lhs, rhs } {};

    std::shared_ptr<ast_term> m_expression[2];       // Left and right

    virtual void show_graph(SUBGRAPH_CONTEXT, node_id parent) override {
        node_id mul = NODE("*"); EDGE(parent, mul);

        for (auto expr: m_expression)
            expr->show_graph(CURRENT_SUBGRAPH_CONTEXT, mul);
    }
};

struct ast_div: public ast_term {
    ast_div(std::shared_ptr<ast_term> lhs,
            std::shared_ptr<ast_term> rhs)
        : m_expression { lhs, rhs } {};

    std::shared_ptr<ast_term> m_expression[2];       // Left and right

    virtual void show_graph(SUBGRAPH_CONTEXT, node_id parent) override {
        node_id div = NODE("/"); EDGE(parent, div);

        for (auto expr: m_expression)
            expr->show_graph(CURRENT_SUBGRAPH_CONTEXT, div);
    }
};

struct ast_add: public ast_expression {
    ast_add(std::shared_ptr<ast_term> lhs,
            std::shared_ptr<ast_expression> rhs)
        : m_lhs(lhs), m_rhs(rhs) {}

    std::shared_ptr<ast_term> m_lhs;
    std::shared_ptr<ast_expression> m_rhs;

    virtual void show_graph(SUBGRAPH_CONTEXT, node_id parent) override {
        node_id add = NODE("+"); EDGE(parent, add);

        m_lhs->show_graph(CURRENT_SUBGRAPH_CONTEXT, add);
        m_rhs->show_graph(CURRENT_SUBGRAPH_CONTEXT, add);
    }
};

struct ast_sub: public ast_expression {
    ast_sub(std::shared_ptr<ast_term> lhs,
            std::shared_ptr<ast_expression> rhs)
        : m_lhs(lhs), m_rhs(rhs) {}

    std::shared_ptr<ast_term> m_lhs;
    std::shared_ptr<ast_expression> m_rhs;

    virtual void show_graph(SUBGRAPH_CONTEXT, node_id parent) override {
        node_id sub = NODE("-"); EDGE(parent, sub);

        m_lhs->show_graph(CURRENT_SUBGRAPH_CONTEXT, sub);
        m_rhs->show_graph(CURRENT_SUBGRAPH_CONTEXT, sub);
    }
};

struct ast_cond: public ast {
    ast_cond(std::shared_ptr<ast_expression> lhs,
             std::shared_ptr<ast_expression> rhs)
        : m_expression { lhs, rhs } {}

    std::shared_ptr<ast_expression> m_expression[2]; // Left and right

    virtual void show_graph(SUBGRAPH_CONTEXT, node_id parent) override {
        m_expression[0]->show_graph(CURRENT_SUBGRAPH_CONTEXT, parent);
        m_expression[1]->show_graph(CURRENT_SUBGRAPH_CONTEXT, parent);
    }
};

struct ast_less: public ast_cond {
    using ast_cond::ast_cond;

    virtual void show_graph(SUBGRAPH_CONTEXT, node_id parent) override {
        node_id node = NODE("<"); EDGE(parent, node);
        this->ast_cond::show_graph(CURRENT_SUBGRAPH_CONTEXT, node);
    }
};

struct ast_less_or_equal: public ast_cond {
    using ast_cond::ast_cond;

    virtual void show_graph(SUBGRAPH_CONTEXT, node_id parent) override {
        node_id node = NODE("<="); EDGE(parent, node);
        this->ast_cond::show_graph(CURRENT_SUBGRAPH_CONTEXT, node);
    }
};

struct ast_greater: public ast_cond {
    using ast_cond::ast_cond;

    virtual void show_graph(SUBGRAPH_CONTEXT, node_id parent) override {
        node_id node = NODE(">"); EDGE(parent, node);
        this->ast_cond::show_graph(CURRENT_SUBGRAPH_CONTEXT, node);
    }
}; 

struct ast_greater_or_equal: public ast_cond {
    using ast_cond::ast_cond;

    virtual void show_graph(SUBGRAPH_CONTEXT, node_id parent) override {
        node_id node = NODE(">="); EDGE(parent, node);
        this->ast_cond::show_graph(CURRENT_SUBGRAPH_CONTEXT, node);
    }
};

struct ast_equals: public ast_cond {
    using ast_cond::ast_cond;

    virtual void show_graph(SUBGRAPH_CONTEXT, node_id parent) override {
        node_id node = NODE("=="); EDGE(parent, node);
        this->ast_cond::show_graph(CURRENT_SUBGRAPH_CONTEXT, node);
    }
};

struct ast_not_equals: public ast_cond {
    using ast_cond::ast_cond;

    virtual void show_graph(SUBGRAPH_CONTEXT, node_id parent) override {
        node_id node = NODE("!="); EDGE(parent, node);
        this->ast_cond::show_graph(CURRENT_SUBGRAPH_CONTEXT, node);
    }
};

struct ast_for: public ast_statement {
    ast_for(std::string var_name,
            std::shared_ptr<ast_term> lhs,
            std::shared_ptr<ast_term> rhs,
            std::shared_ptr<ast_body> body)
        : m_var_name(var_name), m_term { lhs, rhs }, m_body(body) {}

    std::string m_var_name;
    std::shared_ptr<ast_term> m_term[2]; // Left and right
    std::shared_ptr<ast_body> m_body;

    virtual void show_graph(SUBGRAPH_CONTEXT, node_id parent) override {
        node_id node = NODE("for %s", m_var_name.c_str());
        EDGE(parent, node);

        node_id ellipsis = NODE(".."); EDGE(node, ellipsis);
        m_term[0]->show_graph(CURRENT_SUBGRAPH_CONTEXT, ellipsis);
        m_term[1]->show_graph(CURRENT_SUBGRAPH_CONTEXT, ellipsis);

        m_body->show_graph(CURRENT_SUBGRAPH_CONTEXT, node);
    }
};

struct ast_while: public ast_statement {
    ast_while(std::shared_ptr<ast_cond> cond, std::shared_ptr<ast_body> body)
        : m_cond(cond), m_body(body) {}

    std::shared_ptr<ast_cond> m_cond;
    std::shared_ptr<ast_body> m_body;

    virtual void show_graph(SUBGRAPH_CONTEXT, node_id parent) override {
        node_id node = NODE("while"); EDGE(parent, node);
        node_id cond = NODE("?"); EDGE(node, cond);

        m_cond->show_graph(CURRENT_SUBGRAPH_CONTEXT, cond);
        m_body->show_graph(CURRENT_SUBGRAPH_CONTEXT, node);
    }
};

struct ast_assignment: public ast_statement {
    ast_assignment(std::shared_ptr<ast_arg> arg,
                   std::shared_ptr<ast_expression> expression)
        : m_arg(arg), m_expression(expression) {}
  
    std::shared_ptr<ast_arg> m_arg;
    std::shared_ptr<ast_expression> m_expression;

    virtual void show_graph(SUBGRAPH_CONTEXT, node_id parent) override {
        node_id assignment = NODE("="); EDGE(parent, assignment);

        m_arg->show_graph(CURRENT_SUBGRAPH_CONTEXT, assignment);
        m_expression->show_graph(CURRENT_SUBGRAPH_CONTEXT, assignment);
    }
};

struct ast_reassignment: public ast_statement {
    ast_reassignment(std::string name, std::shared_ptr<ast_expression> expression)
        : m_name(name), m_expression(expression) {}

    std::string m_name;
    std::shared_ptr<ast_expression> m_expression;

    virtual void show_graph(SUBGRAPH_CONTEXT, node_id parent) override {
        node_id assignment = NODE("%s =", m_name.c_str()); EDGE(parent, assignment);
        m_expression->show_graph(CURRENT_SUBGRAPH_CONTEXT, assignment);
    }
};

struct ast_return: public ast_statement {
    ast_return(std::shared_ptr<ast_expression> expression)
        : m_expression(expression) {}

    std::shared_ptr<ast_expression> m_expression;

    virtual void show_graph(SUBGRAPH_CONTEXT, node_id parent) override {
        node_id node = NODE("return"); EDGE(parent, node);
        m_expression->show_graph(CURRENT_SUBGRAPH_CONTEXT, node);
    }
};

struct ast_program: public ast {
    ast_program(std::vector<std::shared_ptr<ast_function>> functions)
        : m_functions(functions) {};

    std::vector<std::shared_ptr<ast_function>> m_functions;

    virtual void show_graph(SUBGRAPH_CONTEXT, node_id parent) override {
        node_id program = NODE("program"); EDGE(parent, program);

        for (const auto& func: m_functions)
            func->show_graph(CURRENT_SUBGRAPH_CONTEXT, program);
    }
};

struct ast_if: ast_statement {
    ast_if(std::shared_ptr<ast_cond> cond, std::shared_ptr<ast_body> then)
        : m_cond(cond), m_then(then) {};

    std::shared_ptr<ast_cond> m_cond;
    std::shared_ptr<ast_body> m_then;

    virtual void show_graph(SUBGRAPH_CONTEXT, node_id parent) override {
        node_id node = NODE("if"); EDGE(parent, node);
        node_id cond = NODE("?"); EDGE(node, cond);

        m_cond->show_graph(CURRENT_SUBGRAPH_CONTEXT, cond);
        m_then->show_graph(CURRENT_SUBGRAPH_CONTEXT, node);
    }
};
