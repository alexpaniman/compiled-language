#pragma once

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <wchar.h>

#include <map>
#include <set>
#include <vector>
#include <algorithm>

typedef int generic_token_t;

const generic_token_t EMPTY_TOKEN_ID = -1;

struct trie {
    std::map<char, trie*> transition;
    generic_token_t token;

    trie(): token(EMPTY_TOKEN_ID) {};
};

// Structure created to store non-deterministic finite
// state automata that is meant for lexer building, and
// should be compiled to deterministic finite state
// automata before it can be used to perform tokenization
struct raw_trie {
public:
    // Each symbol in a NFSM can correspond to one or
    // more "states", which are represented by a trie
    // and stored in a linked list accordingly:
    std::map<char, std::set<raw_trie*>> transitions;

    // List of tokens that are considered accepted in
    // the current state of the trie:
    std::vector<generic_token_t> accept;

public:
    raw_trie() = default;
};

static inline
void raw_trie_create_transition(char transition_char, raw_trie* from, raw_trie* to) {
    from->transitions[transition_char].insert(to);
}

// TODO: Should probably aggregate "parsers" from all projects and
// separate them in form of a convenient static library

// Basic structure that encapsulates parsing of a regex expression
struct regex_parser {
    const char* regex;
    int current_index;
};

static inline
char regex_parser_current(regex_parser* parser) {
    return parser->regex[parser->current_index];
}

static inline
char regex_parser_next(regex_parser* parser) {
    return parser->regex[parser->current_index ++];
}

static inline
void regex_parser_rewind(regex_parser* parser, int count) {
    parser->current_index -= count;
}

static inline
raw_trie* regex_parse_one_of(raw_trie* root, regex_parser* parser) {
    raw_trie* next_state = new raw_trie();

    char current = '\0', last_transition = '\0';
    if    ((current = regex_parser_next(parser)) != '[') {
        // Single symbol detected, add simple transition:
        raw_trie_create_transition(current, root, next_state);

        // Remember current transition for handling ranges...
        last_transition = current;
        return next_state;
    }

    bool treat_next_as_range = false;

    // Squarely bracketed expression detected, parse it
    // until right ']', creating transition for every char
    while ((current = regex_parser_next(parser)) != ']') {
        if (treat_next_as_range) {
            for (char in_range = last_transition; in_range <= current; ++ in_range)
                raw_trie_create_transition(in_range, root, next_state);

            treat_next_as_range = false;
            continue;
        }

        // Range marker met
        if (current == '-') { // TODO: Allow at the end
            treat_next_as_range = true;
            continue;
        }

        // Simple character met
        raw_trie_create_transition(current, root, next_state);
        last_transition = current;
    }

    // Function finishes with parser set to the symbol after ']':
    return next_state;
}

inline raw_trie* regex_parse_group(raw_trie* root, regex_parser* parser);

inline raw_trie* regex_parse_expression(raw_trie* root, regex_parser* parser);

inline void raw_trie_collect_nodes(raw_trie *target_trie, std::set<raw_trie*>* nodes) {
    // If we have already visited current node, skip it:
    if (target_trie == NULL || nodes->contains(target_trie))
        return;

    // Insert current node, before continuing, to avoid inf loops
    nodes->insert(target_trie);

    // Visit all the nodes nearby, and add them too
    for (auto &[transition_char, target_nodes]: target_trie->transitions) {
        // Traverse every group of transitions, and collect them:
        for (auto target_node: target_nodes)
            raw_trie_collect_nodes(target_node, nodes);
    }
}

inline void trie_collect_nodes(trie *target_trie, std::set<trie*>* nodes) {
    // If we have already visited current node, skip it:
    if (target_trie == NULL || nodes->contains(target_trie))
        return;

    // Insert current node, before continuing, to avoid inf loops
    nodes->insert(target_trie);

    // Visit all the nodes nearby, and add them too
    for (auto &[transition_char, target_nodes]: target_trie->transition)
        trie_collect_nodes(target_nodes, nodes);
}

inline void raw_trie_replace_state(raw_trie* target_trie, raw_trie* from, raw_trie* to) {
    // Initialize list, that will soon contain all the states
    std::set<raw_trie*> nodes;

    // Collect all trie states to it
    raw_trie_collect_nodes(target_trie, &nodes);

    for (auto& trie_node: nodes)
        // Traverse all the transition characters
        for (auto &[transition_char, target_nodes]: trie_node->transitions)
            if (target_nodes.erase(from) != 0) {
                target_nodes.insert(to);

                // This breaks table's contracts!
                // Makes sure to rehash afterwards!

                // TODO: Does it though?
            }
}


inline raw_trie* regex_kleene_transform(raw_trie* begin, raw_trie* end) {
    raw_trie_replace_state(begin, end, begin);

    return begin;
}

inline raw_trie* regex_parse_group(raw_trie* root, regex_parser* parser) {
    char current = '\0';
    if    ((current = regex_parser_next(parser)) != '(') {
        regex_parser_rewind(parser, 1);
        return regex_parse_one_of(root, parser);
    }

    // Parse many groups
    raw_trie* result = regex_parse_expression(root, parser);

    if (regex_parser_next(parser) != ')')
        assert(0);

    return regex_kleene_transform(root, result);
}

inline raw_trie* regex_parse_expression(raw_trie* root, regex_parser* parser) {
    raw_trie* current = root;
    while (true) {
        char current_symbol = regex_parser_current(parser);
        switch (current_symbol) {
        case  ')':
        case '\0':
            break; // Break out of a loop

        default:
            current = regex_parse_group(current, parser);
            continue;
        }

        break; // Break out of loop if finalizing symbol encourted
    }

    return current;
}

inline void trie_create_link(trie* from, trie* to, char transition) {
    from->transition[transition] = to;
}

inline void trie_nfsm_to_dfsm_recursion(raw_trie* nfsm, trie* root,
        std::map<std::set<raw_trie*>, trie*>* replaced_states,
        std::set<trie*>* tries, std::set<raw_trie*>* raw_tries,
        std::vector<generic_token_t>& rule_order) {

    for (auto &[transition_char, target_nodes]: nfsm->transitions) {
        auto found_state = replaced_states->find(target_nodes);

        trie* new_state = NULL;
        if (found_state == replaced_states->end()) {
            new_state = new trie(); tries->insert(new_state);

            raw_trie* new_raw_state = new raw_trie();
            raw_tries->insert(new_raw_state);

            (*replaced_states)[target_nodes] = new_state;

            std::set<generic_token_t> tokens;

            // For every node that can be reached from current
            for (raw_trie* adjacent_node: target_nodes) {
                tokens.insert(adjacent_node->accept.begin(), adjacent_node->accept.end());

                // Take all edges, and for each of them
                for (auto &[subsequent_letter, subsequent_transitions]: adjacent_node->transitions)
                    // Union sets of transitions
                    for (raw_trie *subsequent_node: subsequent_transitions)
                        raw_trie_create_transition(subsequent_letter, new_raw_state, subsequent_node);
            }

            for (auto it = rule_order.rbegin(); it != rule_order.rend(); ++ it)
                if (std::find(tokens.begin(), tokens.end(), *it) != tokens.end())
                    new_state->token = *it;

            // Now transform resulting node recursively, and write result to /new_state/
            trie_nfsm_to_dfsm_recursion(new_raw_state, new_state, replaced_states, tries, raw_tries, rule_order);
        } else new_state = found_state->second;

        // Linke /new_state/ to the current node
        trie_create_link(root, new_state, transition_char);
    }
}

inline void trie_nfsm_to_dfsm(raw_trie* nfsm, trie** new_trie, std::set<trie*>* tries,
                              std::set<raw_trie*>* raw_tries,
                              std::vector<generic_token_t>& rule_order) {

    std::map<std::set<raw_trie*>, trie*> replaced_states;

    *new_trie = new trie();
    trie_nfsm_to_dfsm_recursion(nfsm, *new_trie, &replaced_states, tries, raw_tries, rule_order);
}

inline raw_trie* regex_parse(raw_trie* root, const char* string, generic_token_t id) {
    regex_parser parser = { string, 0 };
    regex_parse_expression(root, &parser)->accept.push_back(id);

    return root;
}
