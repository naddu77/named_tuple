#include <iostream>
#include <algorithm>
#include <any>
#include <iterator>
#include <iostream>
#include <iterator>
#include <string_view>
#include <type_traits>
#include <utility>
#include <tuple>
#include <vector>
#include <ranges>
#include <cassert>
#include <array>
#include <iomanip>

template <auto N>
struct FixedString
{
    char data[N + 1]{};

    static constexpr auto size() noexcept
    {
        return N;
    }

    constexpr explicit(false) FixedString(char const* str)
    {
        std::ranges::copy_n(str, N + 1, std::begin(data));
    }

    constexpr explicit(false) operator std::string_view() const
    {
        return { data, N };
    }
};

template <auto N>
FixedString(char const (&)[N])->FixedString<N - 1>;

template <FixedString Str>
constexpr auto operator""_fs()
{
    return Str;
}

using namespace std::string_view_literals;

static_assert(""sv == ""_fs);
static_assert("name"sv == "name"_fs);

template <FixedString Name, typename TValue>
struct Arg
{
    static constexpr auto name{ Name };

    TValue value{};

    template <typename T>
    constexpr auto operator=(T const& t)
    {
        return Arg<Name, T>{ .value = t };
    }
};

namespace Internal
{
    template <
        typename TDefault, 
        FixedString,
        template <FixedString, typename> typename
    >
    auto MapLookup(...)->TDefault;

    template <
        typename, 
        FixedString TKey,
        template <FixedString, typename> typename TArg, 
        typename TValue
    >
    auto MapLookup(TArg<TKey, TValue>*)->TArg<TKey, TValue>;

    template <
        typename TDefault,
        typename,
        template <typename, typename> typename
    >
    auto MapLookup(...)->TDefault;

    template <
        typename,
        typename TKey,
        template <typename, typename> typename TArg,
        typename TValue
    >
    auto MapLookup(TArg<TKey, TValue>*)->TArg<TKey, TValue>;
}

template <typename T, FixedString TKey, typename TDefault, template <FixedString, typename> typename TArg>
using MapLookup = decltype(Internal::MapLookup<TDefault, TKey, TArg>(static_cast<T*>(nullptr)));

template <typename... Ts>
struct Inherit
    : Ts...
{

};

static_assert(std::is_same_v<void, MapLookup<Inherit<Arg<"price", double>, Arg<"size", int>>, "unknown", void, Arg>>);
static_assert(std::is_same_v<Arg<"price", double>, MapLookup<Inherit<Arg<"price", double>, Arg<"size", int>>, "price", void, Arg>>);
static_assert(std::is_same_v<Arg<"size", int>, MapLookup<Inherit<Arg<"price", double>, Arg<"size", int>>, "size", void, Arg>>);

struct Any
    : std::any
{
    constexpr Any() = default;

    template <typename T>
    constexpr explicit(false) Any(T const& a)
        : std::any{ a }
        , print{ [](std::ostream& os, std::any const& a) -> std::ostream& {
            if constexpr (requires { os << std::any_cast<T>(a); })
            {
                os << std::any_cast<T>(a);
            }
            else if constexpr (requires {
                std::begin(std::any_cast<T>(a));
                std::end(std::any_cast<T>(a));
            })
            {
                auto obj{ std::any_cast<T>(a) };

                for (auto first{ true }; auto const& e : obj)
                {
                    if (!std::exchange(first, false))
                    {
                        os << ',';
                    }

                    os << e;
                }
            }
            else if constexpr (requires { os << std::any_cast<std::decay_t<T const&>>(a); })
            {
                os << std::any_cast<std::decay_t<T const&>>(a);
            }
            else
            {
                os << a.type().name();
            }

            return os;
        } }
    {

    }

    template <typename T>
    constexpr explicit(false) operator T() const
    {
        return std::any_cast<T>(*this);
    }

    friend std::ostream& operator<<(std::ostream& os, Any const& a)
    {
        return a.print(os, a);
    }

private:
    std::ostream& (*print)(std::ostream&, std::any const&) {};
};

template <FixedString Name>
constexpr auto operator""_t()
{
    return Arg<Name, Any>{};
}

template <FixedString Name, typename... Ts>
struct NamedTuple
    : Ts...
{
    static constexpr auto tag_name{ Name };

    static constexpr auto size() noexcept
    {
        return sizeof...(Ts);
    }

    constexpr explicit(true) NamedTuple(Ts&&... ts)
        : Ts{ std::forward<Ts>(ts) }...
    {

    }

    constexpr explicit(true) NamedTuple(auto&&... ts)
        : Ts{ std::forward<decltype(ts)>(ts) }...
    {

    }

    template <typename... OtherTs>
    constexpr NamedTuple& operator=(NamedTuple<Name, OtherTs...>&& other) noexcept
    {
        if (this != &other)
        {
            ((static_cast<Ts&>(*this) = std::move(other)), ...);
        }

        return *this;
    }

    template <typename... OtherTs>
    constexpr NamedTuple& operator=(NamedTuple<Name, OtherTs...> const& other)
    {
        if (this != &other)
        {
            ((static_cast<Ts&>(*this) = other), ...);
        }

        return *this;
    }

    template <typename T, typename TArg = MapLookup<NamedTuple, T::name, void, Arg>>
    constexpr auto const& operator[](T const) const
        requires(not std::is_void_v<TArg>)
    {
        return static_cast<TArg const&>(*this).value;
    }

    template <typename T, typename TArg = MapLookup<NamedTuple, T::name, void, Arg>>
    constexpr auto& operator[](T const)
        requires(not std::is_void_v<TArg>)
    {
        return static_cast<TArg&>(*this).value;
    }

    auto& Assign(auto&&... ts)
    {
        if constexpr ((requires{
            ts.name;
            ts.value;
        } and ...))
        {
            ((static_cast<MapLookup<NamedTuple, ts.name, void, Arg>&>(*this).value = ts.value), ...);
        }
        else
        {
            ((static_cast<Ts&>(*this).value = ts), ...);
        }

        return *this;
    }

    template <std::size_t N>
    auto& get()
    {
        auto id_type{ [] <auto... Ns>(std::index_sequence<Ns...>) {
            return Inherit<std::pair<std::integral_constant<std::size_t, Ns>, Ts>...>{};
        }(std::make_index_sequence<sizeof...(Ts)>{}) };

        using T = typename decltype(Internal::MapLookup<void, std::integral_constant<std::size_t, N>, std::pair>(&id_type))::second_type;

        return static_cast<T&>(*this);
    }

    template <std::size_t N>
    auto const& get() const
    {
        auto id_type{ [] <auto... Ns>(std::index_sequence<Ns...>) {
            return Inherit<std::pair<std::integral_constant<std::size_t, Ns>, Ts>...>{};
        }(std::make_index_sequence<sizeof...(Ts)>{}) };

        using T = typename decltype(Internal::MapLookup<void, std::integral_constant<std::size_t, N>, std::pair>(&id_type))::second_type;

        return static_cast<T const&>(*this);
    }

    friend std::ostream& operator<<(std::ostream& os, NamedTuple const& nt)
    {
        os << std::string_view{ Name } << '{';

        [&] <auto... Ns>(std::index_sequence<Ns...>) {
            ((os << (Ns ? "," : "") << std::string_view{ Ts::name } << ':' << static_cast<MapLookup<NamedTuple, Ts::name, void, Arg> const&>(nt).value), ...);
        }(std::make_index_sequence<sizeof...(Ts)>{});

        os << '}';

        return os;
    }
};

template <typename... Ts>
NamedTuple(Ts...)->NamedTuple<"", Ts...>;

template <FixedString Name = "", typename... Ts>
constexpr auto MakeNamedTuple(Ts... ts)
{
    return NamedTuple<Name, std::remove_cvref_t<Ts>...>(std::forward<Ts>(ts)...);
}

namespace std
{
    template <FixedString Name, typename... Ts>
    struct tuple_size<NamedTuple<Name, Ts...>>
        : std::integral_constant<std::size_t, sizeof...(Ts)>
    {

    };

    template <std::size_t N, FixedString Name, typename... Ts>
    struct tuple_element<N, NamedTuple<Name, Ts...>>
    {
        using type = decltype(std::declval<NamedTuple<Name, Ts...>>().template get<N>());
    };

    template <std::size_t N, FixedString Name, typename... Ts>
    auto const& get(NamedTuple<Name, Ts...>&& nt) noexcept
    {
        return nt.template get<N>();
    }
}

template <typename T, FixedString... Names>
concept Extends = (requires(T t, Arg<Names, Any> name) { t[name]; } and ...);

int main()
{
    // allow empty
    {
        auto const nt{ MakeNamedTuple() };

        static_assert(not [](auto t) { return requires { t[""_t]; }; }(nt));
    }

    // support direct initialization
    {
        auto const nt{ MakeNamedTuple<"Trade">("price"_t = 42, "size"_t = 100) };

        std::cout << nt << std::endl;

        assert(nt["price"_t] == 42);
    }

    // support initializaion
    {
        using Record = NamedTuple<"", Arg<"price", int>, Arg<"size", int>>;

        auto record1{ Record{
            "price"_t = 42,
            "size"_t = 10
        } };

        auto record2{ Record{
            "price"_t = 43,
            "size"_t = 20
        } };

        assert(42 == record1["price"_t]);
        assert(20 == record2["size"_t]);
    }

    // support extends
    {
        using Record = decltype(MakeNamedTuple("price"_t = int{}, "size"_t = std::size_t{}));
        auto record = Record{ 42, 100ul };

        assert(100ul == record["size"_t]);

        static_assert(not Extends<Record, "quantity">);
        static_assert(not Extends<Record, "price", "quantity">);
        static_assert(not Extends<Record, "price", "size", "value">);
        static_assert(not Extends<Record, "price", "size", "value">);

        static_assert(Extends<Record, "price", "size">);
        static_assert(Extends<Record, "size", "price">);

        auto empty{ MakeNamedTuple() };

        static_assert(not Extends<decltype(empty), "name">);

        auto name{ MakeNamedTuple(empty, "name"_t = 42) };

        static_assert(Extends<decltype(name), "name">);

        constexpr auto get_name = [](Extends<"name"> auto const& t) {
            return t["name"_t];
        };

        assert(42 == get_name(name));
    }

    // support assignment
    {
        auto nt{ MakeNamedTuple("price"_t = int{}, "size"_t = std::size_t{}) };

        assert(0 == nt["price"_t] and 0ul == nt["size"_t]);

        nt.Assign(42, 99ul);

        assert(42 == nt["price"_t] and 99ul == nt["size"_t]);

        nt.Assign("price"_t = 11, "size"_t = 1234);

        assert(11 == nt["price"_t] and std::size_t{ 1234 } == nt["size"_t]);
    }

    // support modification
    {
        auto nt{ MakeNamedTuple("price"_t = int{}, "size"_t = std::size_t{}) };

        nt["price"_t] = 12;
        nt["size"_t] = 34u;

        assert(12 == nt["price"_t] and 34u == nt["size"_t]);
    }

    // support any type
    {
        auto nt{ MakeNamedTuple("price"_t, "size"_t) };

        nt["price"_t] = 12;
        nt["size"_t] = 34u;

        assert(12 == int(nt["price"_t]) and 34 == unsigned(nt["size"_t]));
    }

    // support composition
    {
        auto n1{ MakeNamedTuple("quantity"_t = 42) };
        auto n2{ MakeNamedTuple("value"_t = 100u) };
        auto nt{ MakeNamedTuple<"Msg">(n1, "price"_t, "size"_t, n2) };

        nt["price"_t] = 12;
        nt["size"_t] = 34u;

        assert(12 == int(nt["price"_t]) and 34u == unsigned(nt["size"_t]) and 42 == nt["quantity"_t]);

        std::cout << n1 << std::endl << n2 << std::endl << nt << std::endl;
    }

    // support nesting
    {
        auto nt1{ MakeNamedTuple("first"_t, "last"_t) };
        auto nt2{ MakeNamedTuple<"Attendee">("name"_t = nt1, "position"_t) };

        nt2["name"_t]["first"_t] = "Kris"sv;
        nt2["name"_t]["last"_t] = "Jusiak"sv;
        nt2["position"_t] = "Software Architect"sv;

        std::cout << nt2 << std::endl;
    }

    // get by value
    {
        auto nt{ MakeNamedTuple("price"_t = 100, "size"_t = 42u) };

        assert(100 == nt.get<0>().value and 42u == nt.get<1>().value);
    }

    // support decomposition
    {
        auto nt{ MakeNamedTuple("price"_t = 100, "size"_t = 42u) };
        auto& [price, size] { nt };

        assert(100 == price.value and 42u == size.value);

        price.value = 50;
        size.value = 40u;

        std::cout << nt << std::endl;
    }

    // pack the tuple
    {
        auto nt{ MakeNamedTuple("_1"_t = char{}, "_2"_t = int{}, "_3"_t = char{}) };

        static_assert(12u == sizeof(nt));
    }

    // support array
    {
        auto nt{ MakeNamedTuple<"Person">("name"_t = std::string{}, "children"_t) };

        nt.Assign("name"_t = std::string{ "John" }, "children"_t = Any(std::array{ "Mike", "Keke" }));

        std::cout << nt << std::endl;

        nt.Assign("Mike", std::array{ "John" });

        std::cout << nt << std::endl;
    }

    // apply
    {
        auto nt{ MakeNamedTuple("price"_t = 42, "size"_t = 100) };
        auto f{ [](auto const&... args) {
            std::cout << '{';
            ((std::cout << std::quoted(std::string_view{ args.name }) << " : " << args.value << ','), ...);
            std::cout << '}';
        } };

        [&] <auto... Ns>(std::index_sequence<Ns...>) {
            std::invoke(f, nt.template get<Ns>()...);
        }(std::make_index_sequence<decltype(nt)::size()>{});

        std::cout << std::endl;
    }

    // showcase
    {
        auto employee{ MakeNamedTuple<"Employee">("name"_t, "age"_t, "title"_t) };

        std::vector<decltype(employee)> employees{};

        employees.emplace_back("John", 22, "Software Engineer");
        employees.emplace_back("Michael", 36, "Senior Software Engineer");

        auto age{ 100 };

        employees[0].Assign("age"_t = age);

        auto const to_json{ [](std::ostream& os, auto const& vs) {
            os << "[{\n";

            for (auto const& v : vs)
            {
                os << "\t" << std::quoted(std::string_view{v.tag_name}) << " : {\n";

                [&] <auto... Ns>(std::index_sequence<Ns...>) {
                    ((os << (Ns ? ",\n" : "") << "\t\t" << std::quoted(std::string_view{v.template get<Ns>().name}) << " : " << v.template get<Ns>().value), ...);
                }(std::make_index_sequence<std::remove_cvref_t<decltype(v)>::size()>{});

                os << "\n\t}\n";
            }

            os << "}]\n";
        } };

        to_json(std::cout, employees);
    }
}
