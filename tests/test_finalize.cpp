#include <doctest.h>

#include "descend/args.hpp"
#include "descend/finalize.hpp"

#include <tuple>
#include <utility>

namespace {

namespace dd = descend;
using dd::detail::args;

template <class T>
using finalize_output_type = decltype(dd::detail::finalize(std::declval<T>()));

#define CHECK_FINALIZE_RESULT_TYPE(Result, ...) \
    static_assert(std::is_same_v<finalize_output_type<Result>, __VA_ARGS__>)

using args1 = args<int, const long, bool&, const double&, float&&, const char&&>;

using tuple1 = std::tuple<int, const long, bool&, const double&, float&&, const char&&>;

using pair1 = std::pair<int, const long>;
using pair2 = std::pair<bool&, const double&>;
using pair3 = std::pair<float&&, const char&&>;

TEST_CASE("finalize() for args")
{
    CHECK_FINALIZE_RESULT_TYPE(      args1  , args<int, long, bool, double, float, char>);
    CHECK_FINALIZE_RESULT_TYPE(      args1& , args<int, long, bool, double, float, char>);
    CHECK_FINALIZE_RESULT_TYPE(      args1&&, args<int, long, bool, double, float, char>);
    CHECK_FINALIZE_RESULT_TYPE(const args1& , args<int, long, bool, double, float, char>);
    CHECK_FINALIZE_RESULT_TYPE(const args1&&, args<int, long, bool, double, float, char>);

    {
        using args_of_tuples1 = args<tuple1, const tuple1, tuple1&, const tuple1&, tuple1&&, const tuple1&&>;
        using t = std::tuple<int, long, bool, double, float, char>;

        CHECK_FINALIZE_RESULT_TYPE(args_of_tuples1, args<t, t, t, t, t, t>);
    }

    {
        using int_ref = std::reference_wrapper<int>;
        using const_long_ref = std::reference_wrapper<const long>;

        {
            using args_of_ref_wrappers = args<int_ref, const_long_ref>;
            CHECK_FINALIZE_RESULT_TYPE(args_of_ref_wrappers, args<int&, const long&>);
        }
        {
            using args_of_ref_wrappers = args<const int_ref&, const_long_ref &>;
            CHECK_FINALIZE_RESULT_TYPE(args_of_ref_wrappers, args<int&, const long&>);
        }
        {
            using args_of_ref_wrappers = args<const int_ref&&, const_long_ref &&>;
            CHECK_FINALIZE_RESULT_TYPE(args_of_ref_wrappers, args<int&, const long&>);
        }
    }
    {
        using pair1_ref = std::reference_wrapper<pair1>;
        using pair2_ref = std::reference_wrapper<pair2>;
        using pair3_ref = std::reference_wrapper<pair3>;

        {
            using args_of_ref_wrappers = args<pair1_ref, const pair2_ref, pair3_ref &>;
            CHECK_FINALIZE_RESULT_TYPE(args_of_ref_wrappers, args<pair1&, pair2&, pair3&>);
        }

        {
            using args_of_ref_wrappers = args<const pair1_ref&, pair2_ref &&, const pair3_ref&&>;
            CHECK_FINALIZE_RESULT_TYPE(args_of_ref_wrappers, args<pair1&, pair2&, pair3&>);
        }
    }
    {
        using pair1_ref = std::reference_wrapper<const pair1>;
        using pair2_ref = std::reference_wrapper<const pair2>;
        using pair3_ref = std::reference_wrapper<const pair3>;

        {
            using args_of_ref_wrappers = args<pair1_ref, const pair2_ref, pair3_ref &>;
            CHECK_FINALIZE_RESULT_TYPE(args_of_ref_wrappers, args<const pair1&, const pair2&, const pair3&>);
        }

        {
            using args_of_ref_wrappers = args<const pair1_ref&, pair2_ref &&, const pair3_ref&&>;
            CHECK_FINALIZE_RESULT_TYPE(args_of_ref_wrappers, args<const pair1&, const pair2&, const pair3&>);
        }
    }
}

TEST_CASE("finalize() for tuple")
{
    CHECK_FINALIZE_RESULT_TYPE(      tuple1  , std::tuple<int, long, bool, double, float, char>);
    CHECK_FINALIZE_RESULT_TYPE(      tuple1& , std::tuple<int, long, bool, double, float, char>);
    CHECK_FINALIZE_RESULT_TYPE(      tuple1&&, std::tuple<int, long, bool, double, float, char>);
    CHECK_FINALIZE_RESULT_TYPE(const tuple1& , std::tuple<int, long, bool, double, float, char>);
    CHECK_FINALIZE_RESULT_TYPE(const tuple1&&, std::tuple<int, long, bool, double, float, char>);

    {
        using tuple_of_pairs1 = std::tuple<pair1, const pair1, pair1&, const pair1&, pair1&&, const pair1&&>;
        using p = std::pair<int, long>;

        CHECK_FINALIZE_RESULT_TYPE(tuple_of_pairs1, std::tuple<p, p, p, p, p, p>);
    }

    {
        using tuple_of_pairs2 = std::tuple<pair2, const pair2, pair2&, const pair2&, pair2&&, const pair2&&>;
        using p = std::pair<bool, double>;

        CHECK_FINALIZE_RESULT_TYPE(tuple_of_pairs2, std::tuple<p, p, p, p, p, p>);
    }

    {
        using tuple_of_pairs3 = std::tuple<pair3, const pair3, pair3&, const pair3&, pair3&&, const pair3&&>;
        using p = std::pair<float, char>;

        CHECK_FINALIZE_RESULT_TYPE(tuple_of_pairs3, std::tuple<p, p, p, p, p, p>);
    }
}

TEST_CASE("finalize() for pair")
{
    CHECK_FINALIZE_RESULT_TYPE(      pair1  , std::pair<int, long>);
    CHECK_FINALIZE_RESULT_TYPE(      pair1& , std::pair<int, long>);
    CHECK_FINALIZE_RESULT_TYPE(      pair1&&, std::pair<int, long>);
    CHECK_FINALIZE_RESULT_TYPE(const pair1& , std::pair<int, long>);
    CHECK_FINALIZE_RESULT_TYPE(const pair1&&, std::pair<int, long>);

    CHECK_FINALIZE_RESULT_TYPE(      pair2  , std::pair<bool, double>);
    CHECK_FINALIZE_RESULT_TYPE(      pair2& , std::pair<bool, double>);
    CHECK_FINALIZE_RESULT_TYPE(      pair2&&, std::pair<bool, double>);
    CHECK_FINALIZE_RESULT_TYPE(const pair2& , std::pair<bool, double>);
    CHECK_FINALIZE_RESULT_TYPE(const pair2&&, std::pair<bool, double>);

    CHECK_FINALIZE_RESULT_TYPE(      pair3  , std::pair<float, char>);
    CHECK_FINALIZE_RESULT_TYPE(      pair3& , std::pair<float, char>);
    CHECK_FINALIZE_RESULT_TYPE(      pair3&&, std::pair<float, char>);
    CHECK_FINALIZE_RESULT_TYPE(const pair3& , std::pair<float, char>);
    CHECK_FINALIZE_RESULT_TYPE(const pair3&&, std::pair<float, char>);
}

TEST_CASE("finalize() for others")
{
    using result1 = int;
    CHECK_FINALIZE_RESULT_TYPE(      result1  , int);
    CHECK_FINALIZE_RESULT_TYPE(      result1& , int);
    CHECK_FINALIZE_RESULT_TYPE(      result1&&, int);
    CHECK_FINALIZE_RESULT_TYPE(const result1& , int);
    CHECK_FINALIZE_RESULT_TYPE(const result1&&, int);

    using result2 = std::vector<std::pair<const int, bool>>;
    CHECK_FINALIZE_RESULT_TYPE(      result2  , std::vector<std::pair<const int, bool>>);
    CHECK_FINALIZE_RESULT_TYPE(      result2& , std::vector<std::pair<const int, bool>>);
    CHECK_FINALIZE_RESULT_TYPE(      result2&&, std::vector<std::pair<const int, bool>>);
    CHECK_FINALIZE_RESULT_TYPE(const result2& , std::vector<std::pair<const int, bool>>);
    CHECK_FINALIZE_RESULT_TYPE(const result2&&, std::vector<std::pair<const int, bool>>);

    using result3 = std::reference_wrapper<int>;
    CHECK_FINALIZE_RESULT_TYPE(      result3  , result3);
    CHECK_FINALIZE_RESULT_TYPE(      result3& , result3);
    CHECK_FINALIZE_RESULT_TYPE(      result3&&, result3);
    CHECK_FINALIZE_RESULT_TYPE(const result3& , result3);
    CHECK_FINALIZE_RESULT_TYPE(const result3&&, result3);
}

} // namespace anonymous
