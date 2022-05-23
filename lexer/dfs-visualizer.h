#pragma once

#include "aho.h"

#include "graphviz.h"
#include "hash-set.h"

#include <string>

void trie_vis_create_nodes(SUBGRAPH_CONTEXT, trie* graph,
                           hash_table<trie*, node_id>* nodes,
                           const std::map<generic_token_t, std::string>& token_names, trie* current);

void trie_vis_create_edges(SUBGRAPH_CONTEXT, trie* graph,
                           hash_table<trie*, node_id>* nodes,
                           hash_set<trie*>* visited_nodes);

void trie_vis_create_graph(SUBGRAPH_CONTEXT, trie* graph,
                           const std::map<generic_token_t, std::string>& token_names, trie* current); 

digraph trie_vis_create_graph(trie* graph, const std::map<generic_token_t, std::string>& token_names = {}, trie* current = NULL);
