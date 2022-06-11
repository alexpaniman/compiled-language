#include "lexer.h"
#include "../impl/definitions.h"

#include "graphviz.h"

#include <utility>
#include <iterator>
#include <memory>
#include <optional>
#include <iostream>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>
#include <variant>
#include <functional>

namespace lang {

    //------------------------------------------------------------------------------

    using lexem_iterator = std::vector<lang::lexem>::iterator;

    //------------------------------------------------------------------------------

    static constexpr bool show_utility_nodes = false;

    //------------------------------------------------------------------------------

    template <typename result_type>
    class parser {
    public:
        virtual std::optional<result_type> parse(lexem_iterator& lexems) = 0;

        virtual node_id connect_node(SUBGRAPH_CONTEXT, std::map<void*, node_id>& graphed) {
            node_id this_node = 0;
            if (graphed.contains(this))
                this_node = graphed[this];
            else {
                node saved_node = DEFAULT_NODE;
                style(DEFAULT_NODE);

                this_node = NODE("%s", node_name().c_str());
                graphed[this] = this_node;

                DEFAULT_NODE = saved_node; // Restore style before

                connect_children(CURRENT_SUBGRAPH_CONTEXT, graphed, this_node);
            }

            return this_node;
        }

        digraph draw_graph() {
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

                    std::map<void*, node_id> graphed;
                    connect_node(CURRENT_SUBGRAPH_CONTEXT, graphed);
                });
            });
        }

        virtual ~parser() = default;

        virtual std::string node_name() = 0; // Define name for the parser
        virtual void style(node& default_node) {}

        virtual void connect_children(SUBGRAPH_CONTEXT, std::map<void*, node_id>& graphed, node_id current) = 0;
    };

    //------------------------------------------------------------------------------

    template <typename input_type, typename result_type>
    class unary_parser: public parser<result_type> {
    public:
        unary_parser(parser<input_type>& parser): m_parser(parser) {};

        void connect_children(SUBGRAPH_CONTEXT, std::map<void*, node_id>& graphed, node_id current) override {
            EDGE(current, m_parser.connect_node(CURRENT_SUBGRAPH_CONTEXT, graphed));
        }

    protected:
        parser<input_type>& m_parser;
    };

    //------------------------------------------------------------------------------

    class ignore {}; // Marker for ignored parsers

    template <typename type>
    class ignored_p: public unary_parser<type, ignore> {
    public:
        using unary_parser<type, ignore>::unary_parser;

        std::optional<ignore> parse(lexem_iterator &lexems) override {
            if (this->m_parser.parse(lexems))
                return ignore {};
            else
                return std::nullopt;
        }

        node_id connect_node(SUBGRAPH_CONTEXT, std::map<void*, node_id>& graphed) override {
            if (!show_utility_nodes)
                return this->m_parser.connect_node(CURRENT_SUBGRAPH_CONTEXT, graphed);

            return this->parser<ignore>::connect_node(CURRENT_SUBGRAPH_CONTEXT, graphed);
        }

        void connect_children(SUBGRAPH_CONTEXT, std::map<void*, node_id>& graphed, node_id current) override {
            EDGE(current, this->m_parser.connect_node(CURRENT_SUBGRAPH_CONTEXT, graphed));
        };

        std::string node_name() override { return "(ignore)"; }
    };

    //------------------------------------------------------------------------------

    template <typename repeated_type>
    class many_p: public unary_parser<repeated_type, std::vector<repeated_type>> {
    public:
        using unary_parser<repeated_type, std::vector<repeated_type>>::unary_parser;

        std::optional<std::vector<repeated_type>> parse(lexem_iterator& lexems) override {
            std::vector<repeated_type> parsed_values;
            while (true) {
                auto parsed_value = this->m_parser.parse(lexems);
                if (!parsed_value)
                    break;

                parsed_values.push_back(*parsed_value);
            }

            return parsed_values;
        }

        std::string node_name() override { return "*"; }
    };

    //------------------------------------------------------------------------------

    template <typename type>
    class optional_p: public unary_parser<type, std::optional<type>> {
    public:
        using unary_parser<type, std::optional<type>>::unary_parser;

        std::optional<std::optional<type>> parse(lexem_iterator& lexems) override {
            auto&& parsed_value = this->m_parser.parse(lexems);
            if (!parsed_value) {
                std::optional<type> result = std::nullopt;
                return std::optional(result);
            }

            return *parsed_value;
        }

        std::string node_name() override { return "?"; }
    };

    //------------------------------------------------------------------------------

    template <typename type>
    class lazy_p: public parser<type> {
    public:
        lazy_p(): m_parser(nullptr) {};

        std::optional<type> parse(lexem_iterator& lexems) override {
            return m_parser->parse(lexems);
        }

        void assign(parser<type>& parser_instance) {
            m_parser = &parser_instance;
        }

        operator bool() {
            return m_parser != nullptr;
        }

        std::string node_name() override { return "(lazy)"; }
        node_id connect_node(SUBGRAPH_CONTEXT, std::map<void*, node_id>& graphed) override {
            if (!show_utility_nodes)
                return m_parser->connect_node(CURRENT_SUBGRAPH_CONTEXT, graphed);

            return this->parser<type>::connect_node(CURRENT_SUBGRAPH_CONTEXT, graphed);
        }

        void connect_children(SUBGRAPH_CONTEXT, std::map<void*, node_id>& graphed, node_id current) override {
            EDGE(current, m_parser->connect_node(CURRENT_SUBGRAPH_CONTEXT, graphed));
        };

    private:
        parser<type>* m_parser;
    };

    //------------------------------------------------------------------------------

    template <typename original_type, typename transformed_type>
    class transform_p: public parser<transformed_type> {
    public:
        transform_p(parser<original_type>& parser, transformed_type (*transform)(original_type))
            : m_parser(parser), m_transform(transform) {};

        std::optional<transformed_type> parse(lexem_iterator& lexems) override {
            auto parsed_value = m_parser.parse(lexems);
            if (!parsed_value)
                return std::nullopt;

            return m_transform(*parsed_value);
        }

        node_id connect_node(SUBGRAPH_CONTEXT, std::map<void*, node_id>& graphed) override {
            if (!show_utility_nodes)
                return m_parser.connect_node(CURRENT_SUBGRAPH_CONTEXT, graphed);

            return this->parser<transformed_type>::connect_node(CURRENT_SUBGRAPH_CONTEXT, graphed);
        }

        void connect_children(SUBGRAPH_CONTEXT, std::map<void*, node_id>& graphed, node_id current) override {
            EDGE(current, m_parser.connect_node(CURRENT_SUBGRAPH_CONTEXT, graphed));
        };

        std::string node_name() override { return "(transform)"; }

    private:
        parser<original_type>& m_parser;
        transformed_type (*m_transform)(original_type);
    };

    //------------------------------------------------------------------------------

    template <typename type_0, typename type_1, typename result_type>
    class binary_parser: public parser<result_type> {
    public:
        binary_parser(parser<type_0>& first_parser, parser<type_1>& second_parser)
            : m_parser_0(first_parser), m_parser_1(second_parser) {};

        void connect_children(SUBGRAPH_CONTEXT, std::map<void*, node_id>& graphed, node_id current) override {
            LABELED_EDGE(current, m_parser_0.connect_node(CURRENT_SUBGRAPH_CONTEXT, graphed), "LHS");
            LABELED_EDGE(current, m_parser_1.connect_node(CURRENT_SUBGRAPH_CONTEXT, graphed), "RHS");
        }

    protected:
        parser<type_0>& m_parser_0;
        parser<type_1>& m_parser_1;
    };

    //------------------------------------------------------------------------------

    template <typename left_type, typename right_type>
    static std::optional<std::tuple<left_type, right_type>>
        parser_and(parser<left_type>&   left_parser,
                   parser<right_type>& right_parser, lexem_iterator& lexems) {

        lexem_iterator saved_iterator = lexems;

        auto try_to_parse_fst = left_parser.parse(lexems);
        if (!try_to_parse_fst) {
            lexems = saved_iterator;
            return std::nullopt;
        }

        auto try_to_parse_snd = right_parser.parse(lexems);
        if (!try_to_parse_snd) {
            lexems = saved_iterator;
            return std::nullopt;
        }

        return std::tuple(*try_to_parse_fst, *try_to_parse_snd);
    }


    template <typename... tuple_types>
    constexpr auto strip_empty(std::tuple<tuple_types...> tuple) {
        if constexpr (sizeof...(tuple_types) == 1)
            return std::get<0>(tuple);
        else
            return tuple;
    }

    template <typename type>
    constexpr auto remove_ignored(type value) {
        if constexpr (std::is_same_v<type, ignore>)
            return std::tuple<>();
        else
            return std::tuple(value);
    }

    template <typename... types>
    constexpr auto remove_ignored(std::tuple<types...> tuple) {
        return std::apply([](auto... args) {
            return std::tuple_cat(remove_ignored(args)...);
        }, tuple);
    }

    template <typename... types>
    constexpr auto and_combined_tuple(std::tuple<types...> tuple) {
        return strip_empty(remove_ignored(tuple));
    }

    template <typename type_0, typename type_1, typename result_type>
    class base_and_p: public binary_parser<type_0, type_1, result_type> {
    public:
        using binary_parser<type_0, type_1, result_type>::binary_parser;

        std::string node_name() override { return "&"; }

        void style(node& default_node) override {
            default_node.color = GRAPHVIZ_RED;
        }
    };

    template <typename... types>
    using and_return_t = decltype(and_combined_tuple(std::declval<std::tuple<types...>>()));

    template <typename type_0, typename type_1>
    class and_p: public base_and_p<type_0, type_1, and_return_t<type_0, type_1>> {
    public:
        using base_and_p<type_0, type_1, and_return_t<type_0, type_1>>::base_and_p;

        std::optional<and_return_t<type_0, type_1>> parse(lexem_iterator& lexems) override {
            auto try_to_parse = parser_and(this->m_parser_0, this->m_parser_1, lexems);
            if (!try_to_parse)
                return std::nullopt;

            return and_combined_tuple(*try_to_parse);
        }
    };

    //------------------------------------------------------------------------------

    template <typename type, typename... types>
    struct unique: std::type_identity<type> {};

    template <typename... packed_types, typename current_type, typename... left_types>
    struct unique<std::variant<packed_types...>, current_type, left_types...>
        : std::conditional_t<(std::is_same_v<current_type, packed_types> || ...),
                             unique<std::variant<packed_types...>,               left_types...>,
                             unique<std::variant<packed_types..., current_type>, left_types...>> {};

    template <typename... types>
    using unique_variant = typename unique<std::variant<>, types...>::type;

    //------------------------------------------------------------------------------

    template <typename left_type, typename right_type>
    static std::optional<unique_variant<left_type, right_type>>
        parser_or(parser<left_type>&   left_parser,
                  parser<right_type>& right_parser, lexem_iterator& lexems) {

        lexem_iterator saved_iterator = lexems;

        auto try_to_parse_fst = left_parser.parse(lexems);
        if (try_to_parse_fst)
            return *try_to_parse_fst;

        lexems = saved_iterator;
        auto try_to_parse_snd = right_parser.parse(lexems);
        if (try_to_parse_snd)
            return *try_to_parse_snd;

        lexems = saved_iterator;
        return std::nullopt;
    }

    //------------------------------------------------------------------------------

    template <typename type_0, typename type_1, typename result_type>
    class base_or_p: public binary_parser<type_0, type_1, result_type> {
    public:
        using binary_parser<type_0, type_1, result_type>::binary_parser;

        std::string node_name() override { return "|"; }

        void style(node& default_node) override {
            default_node.color = GRAPHVIZ_ORANGE;
        }
    };

    template <typename type_0, typename type_1>
    class or_p: public base_or_p<type_0, type_1, unique_variant<type_0, type_1>> {
    public:
        using base_or_p<type_0, type_1, unique_variant<type_0, type_1>>::base_or_p;

        std::optional<unique_variant<type_0, type_1>> parse(lexem_iterator& lexems) override {
            return parser_or(this->m_parser_0, this->m_parser_1, lexems);
        }

        std::string node_name() override { return "|"; }
    };

    //------------------------------------------------------------------------------

    template <class... args>
    struct variant_cast_proxy {
        std::variant<args...> m_variant;

        template <class... to_args>
        operator std::variant<to_args...>() const {
            return std::visit([](auto&& arg) -> std::variant<to_args...> { return arg; }, m_variant);
        }
    };

    template <class... args>
    auto variant_cast(const std::variant<args...>& v) -> variant_cast_proxy<args...> { return { v }; }

    //------------------------------------------------------------------------------

    template <typename type_1, typename... type_0s>
    using flatten_variant_parser = base_or_p<std::variant<type_0s...>, type_1,
                                             unique_variant<type_0s..., type_1>>;

    template <typename type_1, typename... type_0s>
    class or_p<std::variant<type_0s...>, type_1>: public flatten_variant_parser<type_1, type_0s...> {

    public:
        using flatten_variant_parser<type_1, type_0s...>::flatten_variant_parser;

        std::optional<unique_variant<type_0s..., type_1>> parse(lexem_iterator& lexems) override {
            auto parser_result = parser_or(this->m_parser_0, this->m_parser_1, lexems);
            if (!parser_result)
                return std::nullopt;

            if (std::holds_alternative<type_1>(*parser_result))
                return std::get<1>(*parser_result);

            return variant_cast(std::get<0>(*parser_result));
        }

        std::string node_name() override { return "|"; }
    };

    //------------------------------------------------------------------------------

    template<typename return_value>
    class parser_w {
    public:
        typedef return_value target_parser_type; // For use in generic operators

        std::vector<std::shared_ptr<void>> m_subject_parsers;

        parser_w(parser<return_value>& parser)
            : m_subject_parsers(), m_parser(parser) {}


        template <typename parser_type>
        parser_w<return_value>& own(parser_w<parser_type>& parser) {
            for (auto&& subject_parser: parser.m_subject_parsers)
                m_subject_parsers.push_back(std::move(subject_parser));
            return *this;
        }

        template <typename... parser_types>
        parser_w<return_value>& own(parser_w<parser_types>&... parsers) {
            ([&](){ own(parsers); } (), ...);
            return *this;
        }

        parser_w<return_value>& own(std::shared_ptr<void> parser) {
            m_subject_parsers.push_back(std::move(parser));
            return *this;
        }

        std::optional<return_value> parse(lexem_iterator& lexems) {
            return m_parser.parse(lexems);
        }

        digraph graph() {
            return m_parser.draw_graph();
        }

        parser<return_value>& raw() { return m_parser; }

        parser_w<return_value>& set(parser_w<return_value>& init) {
            // This gives up compile-time safety for lazy_p :(, but I haven't
            // came up with compile-time safe and convenient implementation

            dynamic_cast<lazy_p<return_value>&>(m_parser).assign(init.m_parser);
            // And also this operator can be a bit misleading, but we're building
            // DSL anyway, so it's probably fine.

            return *this;
        }

        parser_w<return_value>& set(parser_w<return_value>&& init) {
            // Same here, but owning for rvalue references

            dynamic_cast<lazy_p<return_value>&>(m_parser).assign(init.m_parser);
            return this->own(init);
        }

    private:
        parser<return_value>& m_parser;
    };

    template <typename type>
    using compatible_parser_return_t =
        typename std::remove_reference_t<type>::target_parser_type;

    template <typename type>
    concept compatible_parser_w = std::is_convertible_v<
            std::remove_reference_t<type>,
            parser_w<compatible_parser_return_t<type>>
        >;

    template <typename original_type>
    auto non_owning_transform(lang::parser_w<original_type>&& parser, auto&& transformer) {
        // Get lambda's return type:
        using transformed_type = std::decay_t<decltype(transformer(std::declval<original_type>()))>;
        using transform_parser = transform_p<original_type, transformed_type>;

        auto&& new_parser = std::make_shared<transform_parser>(parser.raw(), transformer);
        return parser_w<transformed_type>(*new_parser).own(new_parser);
    }

    template <typename original_type>
    auto transform(lang::parser_w<original_type>&& parser, auto&& transformer) {
        return non_owning_transform(std::move(parser), transformer).own(parser);
    }

    template <typename original_type>
    auto transform(lang::parser_w<original_type>& parser, auto&& transformer) {
        return non_owning_transform(std::move(parser), transformer);
    }

    template <typename constructor, typename... arg_types>
    parser_w<std::shared_ptr<constructor>>
        non_owning_construct(parser_w<std::tuple<arg_types...>>&& parser) {

        return non_owning_transform(std::move(parser), [](auto tree) {
            return std::apply([](auto&&... args) {
                return std::make_shared<constructor>(args...);
            }, tree);
        });
    }

    template <typename constructor, typename arg_type>
    parser_w<std::shared_ptr<constructor>> non_owning_construct(parser_w<arg_type>&& parser) {
        return non_owning_transform(std::move(parser), [](auto tree) {
            return std::make_shared<constructor>(tree);
        });
    }

    template <typename constructor, typename arg_type>
    parser_w<std::shared_ptr<constructor>> construct(parser_w<arg_type>& parser) {
        return non_owning_construct<constructor>(std::move(parser));
    }

    template <typename constructor, typename arg_type>
    parser_w<std::shared_ptr<constructor>> construct(parser_w<arg_type>&& parser) {
        return non_owning_construct<constructor>(std::move(parser)).own(parser);
    }

    template<typename return_value>
    class alloc_p {
    public:
        typedef std::shared_ptr<return_value> target_parser_type; // For use in generic operators

        template <compatible_parser_w input_parser>
        alloc_p(input_parser&& parser)
            : m_parser(construct<return_value>(std::forward<input_parser>(parser))) {}

        operator parser_w<std::shared_ptr<return_value>>() { return m_parser; }

    private:
        parser_w<std::shared_ptr<return_value>> m_parser;
    };

    template <typename target_type, typename original_type>
    auto variant_upcast(parser_w<original_type>&& parser) {
        return transform(std::move(parser), [](auto tree) {
            return std::visit([](auto&& alternative) {
                return std::static_pointer_cast<target_type>(alternative);
            }, tree);
        });
    }

    template <compatible_parser_w repeated_parser>
    auto separated_by(repeated_parser&& repeated, parser_w<ignore>&& separator) {
        using repeated_t = compatible_parser_return_t<repeated_parser>;

        // Grammar: <separated_by> ::= (repeated & (<separator> & repeated)*)?
        auto separated_by_grammar = optional(std::forward<repeated_parser>(repeated)
                                             & many(std::move(separator) & repeated));

        return transform(std::move(separated_by_grammar), [](auto tree) {
            std::vector<repeated_t> values;
            if (tree) { // Collect parsed values
                values.push_back(std::get<0>(*tree));

                for (auto &&arg: std::get<1>(*tree))
                    values.push_back(arg);
            }              

            return values;
        });
    }      

    //------------------------------------------------------------------------------

    template <typename type>
    parser_w<ignore> ignored(parser_w<type>&& ignored) {
        auto&& new_parser = std::make_shared<ignored_p<type>>(ignored.raw());
        return parser_w(*new_parser).own(new_parser).own(ignored);
    }

    template <typename type>
    parser_w<ignore> ignored(parser_w<type>& ignored) {
        auto&& new_parser = std::make_shared<ignored_p<type>>(ignored.raw());
        return parser_w(*new_parser).own(new_parser);
    }

    //------------------------------------------------------------------------------

    template <typename type>
    parser_w<type> lazy() {
        auto&& new_parser = std::make_shared<lazy_p<type>>();
        return parser_w(*new_parser).own(new_parser);
    }

    template <typename type>
    class lazy_w {
    public:
        typedef type target_parser_type; // For use in generic operators

        lazy_w(): m_parser(lazy<type>()) {}

        template <compatible_parser_w parser_type>
        requires std::is_same_v<typename std::remove_reference_t<parser_type>::target_parser_type, type>
        void operator=(parser_type&& parser) {
            m_parser.set(std::forward<parser_type>(parser));
        }

        operator parser_w<type>() { return m_parser; }

    private:
        parser_w<type> m_parser;
    };

    //------------------------------------------------------------------------------

    template <typename type_0, typename type_1>
    static inline auto allocate_and(parser_w<type_0>&& parser_0, parser_w<type_1>&& parser_1) {
        auto&& new_parser = std::make_shared<and_p<type_0, type_1>>(parser_0.raw(), parser_1.raw());
        return parser_w(*new_parser).own(new_parser);
    }

    //------------------------------------------------------------------------------

    template <typename type, typename subject_parser_type>
    void take_ownership(parser_w<type>& owning, subject_parser_type&& subject) {
        if constexpr (std::is_rvalue_reference_v<decltype(subject)>)
            owning.own(subject);
    }

    template <compatible_parser_w type_0, compatible_parser_w type_1>
    auto operator&(type_0&& parser_0, type_1&& parser_1) {
        using parser_0_type = typename std::remove_reference_t<type_0>::target_parser_type;
        using parser_1_type = typename std::remove_reference_t<type_1>::target_parser_type;

        auto&& new_parser =
            std::make_shared<and_p<parser_0_type, parser_1_type>>(
                static_cast<parser_w<parser_0_type>>(parser_0).raw(),
                static_cast<parser_w<parser_1_type>>(parser_1).raw()
            );

        auto new_parser_w = parser_w(*new_parser).own(new_parser);

        take_ownership(new_parser_w, std::forward<type_0>(parser_0));
        take_ownership(new_parser_w, std::forward<type_1>(parser_1));

        return new_parser_w;
    }

    // template <typename type_0, typename type_1>
    // auto operator&(parser_w<type_0>& parser_0, parser_w<type_1>& parser_1) {
    //     return allocate_and(std::move(parser_0), std::move(parser_1));
    // }

    // template <typename type_0, typename type_1>
    // auto operator&(parser_w<type_0>&& parser_0, parser_w<type_1>& parser_1) {
    //     return allocate_and(std::move(parser_0), std::move(parser_1)).own(parser_0);
    // }

    // template <typename type_0, typename type_1>
    // auto operator&(parser_w<type_0>& parser_0, parser_w<type_1>&& parser_1) {
    //     return allocate_and(std::move(parser_0), std::move(parser_1)).own(parser_1);
    // }

    // template <typename type_0, typename type_1>
    // auto operator&(parser_w<type_0>&& parser_0, parser_w<type_1>&& parser_1) {
    //     return allocate_and(std::move(parser_0), std::move(parser_1))
    //         .own(parser_0, parser_1);
    // }

    //------------------------------------------------------------------------------

    template <typename type_0, typename type_1>
    static inline auto allocate_or(parser_w<type_0>&& parser_0, parser_w<type_1>&& parser_1) {
        auto&& new_parser = std::make_shared<or_p<type_0, type_1>>(parser_0.raw(), parser_1.raw());
        return parser_w(*new_parser).own(new_parser);
    }

    //------------------------------------------------------------------------------

    template <compatible_parser_w type_0, compatible_parser_w type_1>
    auto operator|(type_0&& parser_0, type_1&& parser_1) {
        using parser_0_type = typename std::remove_reference_t<type_0>::target_parser_type;
        using parser_1_type = typename std::remove_reference_t<type_1>::target_parser_type;

        auto&& new_parser =
            std::make_shared<or_p<parser_0_type, parser_1_type>>(
                static_cast<parser_w<parser_0_type>>(parser_0).raw(),
                static_cast<parser_w<parser_1_type>>(parser_1).raw()
            );

        auto new_parser_w = parser_w(*new_parser).own(new_parser);

        take_ownership(new_parser_w, std::forward<type_0>(parser_0));
        take_ownership(new_parser_w, std::forward<type_1>(parser_1));

        return new_parser_w;
    }

    //------------------------------------------------------------------------------

    // template <typename type_0, typename type_1>
    // auto operator|(parser_w<type_0>& parser_0, parser_w<type_1>& parser_1) {
    //     return allocate_or(std::move(parser_0), std::move(parser_1));
    // }

    // template <typename type_0, typename type_1>
    // auto operator|(parser_w<type_0>&& parser_0, parser_w<type_1>& parser_1) {
    //     return allocate_or(std::move(parser_0), std::move(parser_1)).own(parser_0);
    // }

    // template <typename type_0, typename type_1>
    // auto operator|(parser_w<type_0>& parser_0, parser_w<type_1>&& parser_1) {
    //     return allocate_or(std::move(parser_0), std::move(parser_1)).own(parser_1);
    // }

    // template <typename type_0, typename type_1>
    // auto operator|(parser_w<type_0>&& parser_0, parser_w<type_1>&& parser_1) {
    //     return allocate_or(std::move(parser_0), std::move(parser_1))
    //         .own(parser_0, parser_1);
    // }

    //------------------------------------------------------------------------------

    template <typename parsed_type>
    static inline auto allocate_many(parser_w<parsed_type>&& parser) {
    }

    //------------------------------------------------------------------------------

    template <compatible_parser_w parser_type>
    auto many(parser_type&& parser) {
        using parsed_type = compatible_parser_return_t<parser_type>;

        parser_w<parsed_type> wrapped_parser = parser;

        auto&& new_parser = std::make_shared<many_p<parsed_type>>(wrapped_parser.raw());
        return parser_w(*new_parser).own(new_parser).own(wrapped_parser);
    }

    //------------------------------------------------------------------------------

    template <typename parsed_type>
    static inline auto allocate_optional(parser_w<parsed_type>&& parser) {
        auto&& new_parser = std::make_shared<optional_p<parsed_type>>(parser.raw());
        return parser_w(*new_parser).own(new_parser);
    }

    //------------------------------------------------------------------------------

    template <typename parsed_type>
    auto optional(parser_w<parsed_type>& parser) {
        return allocate_optional(std::move(parser));
    }

    template <typename parsed_type>
    auto optional(parser_w<parsed_type>&& parser) {
        return allocate_optional(std::move(parser)).own(parser);
    }

    //------------------------------------------------------------------------------

    class lexem_parser_p: public parser<lang::lexem> {
    public:
        lexem_parser_p(named_lexem id);
        std::optional<lang::lexem> parse(lexem_iterator& lexems) override;

    private:
        named_lexem m_named_token;

        std::string node_name() override;
        void connect_children(SUBGRAPH_CONTEXT, std::map<void*, node_id>& graphed, node_id current) override;
        
        void style(node& default_node) override;
    };

    parser_w<lexem>  static_parser(named_lexem id);
    #define static_p(id) static_parser(lang::named_lexem { id, #id })

    parser_w<ignore> ignore_parser(named_lexem id);
    #define ignore_p(id) ignore_parser(lang::named_lexem { id, #id })

    template <typename type>
    class test: public parser<type> {
        std::optional<type> parse(lexem_iterator& lexems) override {
            return type {};
        }
    };

    template <size_t id>
    struct dummy_t { static constexpr int value = id; };

    template <size_t id>
    auto test_p() {
        auto&& new_parser = std::make_shared<test<dummy_t<id>>>();
        return parser_w(*new_parser).own(new_parser);
    }

}
