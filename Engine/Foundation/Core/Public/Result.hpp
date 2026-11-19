#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

#include "Assert.hpp"

// How a Runic function reports failure: it returns a Result, and no exception crosses a module
// boundary.

namespace Runic::Core {

// Why an operation failed, in words a log or a person can read. Stage A carries no error code: one
// joins when a caller must branch on the kind of failure.
struct Error {
    std::string message;
};

// The usual way to fail: return Fail("no layer named '" + name + "'");
inline Error Fail(std::string message) { return Error{std::move(message)}; }

namespace Detail {
// The asserts of both Result forms.
inline constexpr const char *kValueOfAnErrorMessage = "Result::Value() on an error";
inline constexpr const char *kErrorOfASuccessMessage = "Result::GetError() on a success";
} // namespace Detail

// A value, or the Error that prevented it.
template <class T> class [[nodiscard]] Result {
    static_assert(!std::is_same_v<std::remove_cv_t<T>, Error>, "Result<Error> would be ambiguous");
    static_assert(!std::is_reference_v<T>, "a Result holds a value");

  public:
    // return value;
    Result(T value) : m_State(std::in_place_index<kValueAlternative>, std::move(value)) {}
    // return Fail("...");
    Result(Error error) : m_State(std::in_place_index<kErrorAlternative>, std::move(error)) {}

    bool HasValue() const noexcept { return m_State.index() == kValueAlternative; }
    explicit operator bool() const noexcept { return HasValue(); }

    // The value; asserts that there is one.
    T &Value() & {
        RUNIC_ASSERT_MSG(HasValue(), Detail::kValueOfAnErrorMessage);
        return *std::get_if<kValueAlternative>(&m_State);
    }
    const T &Value() const & {
        RUNIC_ASSERT_MSG(HasValue(), Detail::kValueOfAnErrorMessage);
        return *std::get_if<kValueAlternative>(&m_State);
    }
    T &&Value() && {
        RUNIC_ASSERT_MSG(HasValue(), Detail::kValueOfAnErrorMessage);
        return std::move(*std::get_if<kValueAlternative>(&m_State));
    }

    // The error; asserts that there is one.
    const Error &GetError() const {
        RUNIC_ASSERT_MSG(!HasValue(), Detail::kErrorOfASuccessMessage);
        return *std::get_if<kErrorAlternative>(&m_State);
    }

  private:
    // Which alternative of m_State holds what.
    static constexpr std::size_t kValueAlternative = 0;
    static constexpr std::size_t kErrorAlternative = 1;

    std::variant<T, Error> m_State;
};

// Success with nothing to return, or the Error.
template <> class [[nodiscard]] Result<void> {
  public:
    Result() noexcept = default;                       // return {};
    Result(Error error) : m_Error(std::move(error)) {} // return Fail("...");

    bool HasValue() const noexcept { return !m_Error.has_value(); }
    explicit operator bool() const noexcept { return HasValue(); }

    // The error; asserts that there is one.
    const Error &GetError() const {
        RUNIC_ASSERT_MSG(m_Error.has_value(), Detail::kErrorOfASuccessMessage);
        return *m_Error;
    }

  private:
    std::optional<Error> m_Error;
};

} // namespace Runic::Core
