#include <memory>
#include <optional>
#include <iostream>
#include <tuple>
#include <type_traits>
#include <vector>
#include <variant>
#include <functional>

//------------------------------------------------------------------------------

using lexem_iterator = int;

//------------------------------------------------------------------------------

template <typename result_type>
class parser {
public:
    virtual std::optional<result_type> parse(lexem_iterator& lexems) = 0;
    virtual ~parser() = default;
};

//------------------------------------------------------------------------------

template <typename input_type, typename result_type>
class unary_parser: public parser<result_type> {
public:
    unary_parser(parser<input_type>& parser): m_parser(parser) {};

protected:
    parser<input_type>& m_parser;
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

protected:
    parser<type_0>& m_parser_0;
    parser<type_1>& m_parser_1;
};

template <typename type_1, typename... type_0s>
using flatten_tuple_parser = binary_parser<std::tuple<type_0s...>, type_1,
                                           std::tuple<type_0s...,  type_1>>;

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

//------------------------------------------------------------------------------

template <typename type_0, typename type_1>
class and_p: public binary_parser<type_0, type_1, std::tuple<type_0, type_1>> {
public:
    using base = binary_parser<type_0, type_1, std::tuple<type_0, type_1>>;
    using binary_parser<type_0, type_1, std::tuple<type_0, type_1>>::binary_parser;

    std::optional<std::tuple<type_0, type_1>> parse(lexem_iterator& lexems) override {
        return parser_and(this->m_parser_0, this->m_parser_1, lexems);
    }
};

template <typename type_1, typename... type_0s>
class and_p<std::tuple<type_0s...>, type_1>: public flatten_tuple_parser<type_1, type_0s...> {

public:
    using base = flatten_tuple_parser<type_1, type_0s...>;
    using flatten_tuple_parser<type_1, type_0s...>::flatten_tuple_parser;

    std::optional<std::tuple<type_0s..., type_1>> parse(lexem_iterator& lexems) override {
        auto simple_parser_result = parser_and(this->m_parser_0, this->m_parser_1, lexems);
        if (!simple_parser_result)
            return std::nullopt;

        auto [parsed_first, parsed_second] = *simple_parser_result;

        // Flatten result in one tuple:
        return std::tuple_cat(parsed_first, std::tuple(parsed_second));
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

template <typename... Ts>
using unique_variant = typename unique<std::variant<>, Ts...>::type;

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

template <typename type_0, typename type_1>
class or_p: public binary_parser<type_0, type_1, unique_variant<type_0, type_1>> {
public:
    using binary_parser<type_0, type_1, unique_variant<type_0, type_1>>::binary_parser;

    std::optional<unique_variant<type_0, type_1>> parse(lexem_iterator& lexems) override {
        return parser_or(this->m_parser_0, this->m_parser_1, lexems);
    }
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
using flatten_variant_parser = binary_parser<std::variant<type_0s...>, type_1,
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
};

//------------------------------------------------------------------------------

template<typename return_value>
class parser_w {
public:
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

    parser<return_value>& raw() { return m_parser; }

    parser_w<return_value>& operator=(parser_w<return_value>& init) {
        // This gives up compile-time safety for lazy_p :(, but I haven't
        // came up with compile-time safe and convenient implementation

        dynamic_cast<lazy_p<return_value>&>(m_parser).assign(init.m_parser);
        // And also this operator can be a bit misleading, but we're building
        // DSL anyway, so it's probably fine.

        return *this;
    }

    template <typename transformed_type>
    parser_w<transformed_type> transform(transformed_type (*transform)(return_value)) {
        auto&& new_parser =
            std::make_shared<transform_p<return_value, transformed_type>>(this->raw(), transform);

        return parser_w<transformed_type>(*new_parser).own(new_parser).own(*this);
    }

private:
    parser<return_value>& m_parser;
};

//------------------------------------------------------------------------------

template <typename type>
parser_w<type> lazy() {
    auto&& new_parser = std::make_shared<lazy_p<type>>();
    return parser_w(*new_parser).own(new_parser);
}

//------------------------------------------------------------------------------

template <typename type_0, typename type_1>
static inline auto allocate_and(parser_w<type_0>&& parser_0, parser_w<type_1>&& parser_1) {
    auto&& new_parser = std::make_shared<and_p<type_0, type_1>>(parser_0.raw(), parser_1.raw());
    return parser_w(*new_parser).own(new_parser);
}

//------------------------------------------------------------------------------

template <typename type_0, typename type_1>
auto operator&(parser_w<type_0>& parser_0, parser_w<type_1>& parser_1) {
    return allocate_and(std::move(parser_0), std::move(parser_1));
}

template <typename type_0, typename type_1>
auto operator&(parser_w<type_0>&& parser_0, parser_w<type_1>& parser_1) {
    return allocate_and(std::move(parser_0), std::move(parser_1)).own(parser_0);
}

template <typename type_0, typename type_1>
auto operator&(parser_w<type_0>& parser_0, parser_w<type_1>&& parser_1) {
    return allocate_and(std::move(parser_0), std::move(parser_1)).own(parser_1);
}

template <typename type_0, typename type_1>
auto operator&(parser_w<type_0>&& parser_0, parser_w<type_1>&& parser_1) {
    return allocate_and(std::move(parser_0), std::move(parser_1))
        .own(parser_0, parser_1);
}

//------------------------------------------------------------------------------

template <typename type_0, typename type_1>
static inline auto allocate_or(parser_w<type_0>&& parser_0, parser_w<type_1>&& parser_1) {
    auto&& new_parser = std::make_shared<or_p<type_0, type_1>>(parser_0.raw(), parser_1.raw());
    return parser_w(*new_parser).own(new_parser);
}

//------------------------------------------------------------------------------

template <typename type_0, typename type_1>
auto operator|(parser_w<type_0>& parser_0, parser_w<type_1>& parser_1) {
    return allocate_or(std::move(parser_0), std::move(parser_1));
}

template <typename type_0, typename type_1>
auto operator|(parser_w<type_0>&& parser_0, parser_w<type_1>& parser_1) {
    return allocate_or(std::move(parser_0), std::move(parser_1)).own(parser_0);
}

template <typename type_0, typename type_1>
auto operator|(parser_w<type_0>& parser_0, parser_w<type_1>&& parser_1) {
    return allocate_or(std::move(parser_0), std::move(parser_1)).own(parser_1);
}

template <typename type_0, typename type_1>
auto operator|(parser_w<type_0>&& parser_0, parser_w<type_1>&& parser_1) {
    return allocate_or(std::move(parser_0), std::move(parser_1))
        .own(parser_0, parser_1);
}

//------------------------------------------------------------------------------

template <typename parsed_type>
static inline auto allocate_many(parser_w<parsed_type>&& parser) {
    auto&& new_parser = std::make_shared<many_p<parsed_type>>(parser.raw());
    return parser_w(*new_parser).own(new_parser);
}

//------------------------------------------------------------------------------

template <typename parsed_type>
auto many(parser_w<parsed_type>& parser) {
    return allocate_many(std::move(parser));
}

template <typename parsed_type>
auto many(parser_w<parsed_type>&& parser) {
    return allocate_many(std::move(parser)).own(parser);
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

// Just for testing purposes...
template <typename type>
class const_p: public parser<type> {
public:
    const_p(type value): m_return_value(value) {};

    std::optional<type> parse(lexem_iterator& lexems) override { return m_return_value; }

private:
    type m_return_value;
};

template <typename return_type>
parser_w<return_type> tstatic_p(return_type value) {
    auto&& const_parser = std::make_shared<const_p<return_type>>(value);
    return parser_w(*const_parser).own(const_parser);
}

//------------------------------------------------------------------------------

// Just for testing purposes...
template <typename type>
class false_p: public parser<type> {
public:
    false_p() = default;

    std::optional<type> parse(lexem_iterator& lexems) override { return std::nullopt; }
};

template <typename return_type>
parser_w<return_type> fstatic_p() {
    auto&& const_parser = std::make_shared<false_p<return_type>>();
    return parser_w(*const_parser).own(const_parser);
}


//------------------------------------------------------------------------------

int num = 1;

//------------------------------------------------------------------------------

template <size_t num> 
struct dummy_t {};


int main(void) {
    auto _0 = tstatic_p(dummy_t<0>());
    auto _1 = tstatic_p(dummy_t<1>());

    auto _2 = lazy<const char*>();
    auto _3 = tstatic_p(3).transform<const char*>([](auto a) { return "hello!"; });

    auto _01 = _2 & optional(fstatic_p<int>()) & (fstatic_p<int>() | tstatic_p(3) | (_0 & _1) | _1) & _0;
    _2 = _3;

    std::cout << std::get<0>(*_01.parse(num)) << std::endl;
}
