#include "dfs-visualizer.h"

#include "aho.h"
#include "default-hash-functions.h"
#include "graphviz.h"
#include "lexer.h"
#include "simple-stack.h"
#include "safe-alloc.h"

#include <cstdint>
#include <iostream>
#include <ostream>

void trie_vis_create_nodes(SUBGRAPH_CONTEXT, trie *graph, char& name,
                           hash_table<trie *, node_id> *nodes,
                           const std::map<generic_token_t, std::string>& token_names, trie* current) {

    if (graph == NULL || hash_table_contains(nodes, graph))
        return;

    node_id new_node;

    if (graph == current)
        DEFAULT_NODE.color = GRAPHVIZ_RED;

    else if (graph->token != lang::EMPTY_TOKEN_ID)
        DEFAULT_NODE.color = GRAPHVIZ_BLUE;

    else if (graph->token != lang::IGNORED_TOKEN_ID)
        DEFAULT_NODE.color = GRAPHVIZ_ORANGE;

    else
        DEFAULT_NODE.color = GRAPHVIZ_BLACK;


    if (graph->token == lang::EMPTY_TOKEN_ID || graph->token == lang::IGNORED_TOKEN_ID)
        new_node = NODE("%c", name ++);

    else {
        if (token_names.contains(graph->token))
            new_node = NODE("%s", token_names.at(graph->token).c_str());
        else
            new_node = NODE("%c: %d", name ++, graph->token);
    }

    hash_table_insert(nodes, graph, new_node);

    // Visit all the nodes nearby, and add them too
    for (auto &[transition_char, target]: graph->transition)
        trie_vis_create_nodes(CURRENT_SUBGRAPH_CONTEXT, target, name, nodes, token_names, current);
}

static inline
void escape(simple_stack<char>* string_builder) {
    simple_stack_push(string_builder, '\\');
    simple_stack_push(string_builder, '\\');
}

static inline
void display_char(simple_stack<char>* string_builder, char char_to_display) {
    if (char_to_display == '\n') {
        escape(string_builder);
        simple_stack_push(string_builder, 'n');
        return;
    }

    if (char_to_display == '\t') {
        escape(string_builder);
        simple_stack_push(string_builder, 't');
        return;
    }

    simple_stack_push(string_builder, char_to_display);
}


static inline
void print_range(simple_stack<char>* string_builder, char previous, int streak_length) {
    if (streak_length == 1) {
        display_char(string_builder, previous);
        return;
    }

    display_char(string_builder, previous - streak_length + 1);
    simple_stack_push(string_builder, '-');
    display_char(string_builder, previous);
}

static inline
char* trie_vis_create_transition_description(std::vector<char>& list) {
    std::sort(list.begin(), list.end());

    simple_stack<char> string_builder;
    simple_stack_create(&string_builder);

    if (list.size() > 1)
        simple_stack_push(&string_builder, '[');

    int streak_length = 0;
    char previous = list.front() - 1;

    for (char symbol: list) {
        if (symbol - previous == 1)
            ++ streak_length;
        else {
            print_range(&string_builder, previous, streak_length);
            streak_length = 1;
        }

        previous = symbol;
    }

    print_range(&string_builder, previous, streak_length);

    if (list.size() > 1)
        simple_stack_push(&string_builder, ']');

    simple_stack_push(&string_builder, '\0');

    return string_builder.elements;
}

uint32_t trie_pointer_hash(trie* target_trie) {
    return int_hash((int) (uintptr_t) target_trie);
}

void trie_vis_create_edges(SUBGRAPH_CONTEXT, trie* graph,
                           hash_table<trie*, node_id>* nodes,
                           hash_set<trie*>* visited_nodes) {

    if (hash_set_contains(visited_nodes, graph))
        return;

    hash_set_insert(visited_nodes, graph);

    hash_table<trie*, std::vector<char>> connections;
    hash_table_create(&connections, trie_pointer_hash);

    for (auto &[transition_char, target]: graph->transition) {
        std::vector<char>* list_of_paths =
            hash_table_lookup(&connections, target);

        if (list_of_paths == NULL) {
            std::vector<char> new_list;

            // Copy the list over to the connections hash_table
            hash_table_insert(&connections, target, new_list);
            list_of_paths = hash_table_lookup(&connections, target);
        }

        list_of_paths->push_back(transition_char);
    }

    node_id from = *hash_table_lookup(nodes, graph);
    HASH_TABLE_TRAVERSE(&connections, trie*, std::vector<char>, current) {
        node_id to = *hash_table_lookup(nodes, KEY(current));

        char* edge_description =
            trie_vis_create_transition_description(VALUE(current));

        LABELED_EDGE(from, to, "%s", edge_description);
        free(edge_description), edge_description = NULL;

        trie_vis_create_edges(CURRENT_SUBGRAPH_CONTEXT,
                              KEY(current), nodes, visited_nodes);
    }

    HASH_TABLE_TRAVERSE(&connections, trie*, std::vector<char>, current)
        VALUE(current).~vector();

    hash_table_destroy(&connections);
}

void trie_vis_create_graph(SUBGRAPH_CONTEXT, trie* graph,
                           const std::map<generic_token_t, std::string>& token_names, trie* current) {

    hash_table<trie*, node_id> nodes;
    hash_table_create(&nodes, trie_pointer_hash);

    // Declare all the nodes in the graph
    char node_name = 'A';
    trie_vis_create_nodes(CURRENT_SUBGRAPH_CONTEXT, graph, node_name, &nodes, token_names, current);

    hash_set<trie*> visited_nodes;
    hash_set_create(&visited_nodes, trie_pointer_hash);

    // Create all the edges in the nodes
    trie_vis_create_edges(CURRENT_SUBGRAPH_CONTEXT, graph,
                          &nodes, &visited_nodes);

    // Free all the memory we used so far
    hash_table_destroy(&nodes), nodes = {};
    hash_set_destroy(&visited_nodes), visited_nodes = {};
}

digraph trie_vis_create_graph(trie* graph, const std::map<generic_token_t, std::string>& token_names, trie* current) {
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

            trie_vis_create_graph(CURRENT_SUBGRAPH_CONTEXT, graph, token_names, current);
        });
    });
}
