// Aleth: Ethereum C++ client, tools and libraries.
// Copyright 2014-2019 Aleth Authors.
// Licensed under the GNU General Public License, Version 3.
#pragma once

#include <json/json.h>

#include <string>

#include "CommonData.h"
#include "CommonIO.h"
#include "FixedHash.h"

namespace dev {
inline std::string toJS(::byte _b) { return "0x" + std::to_string(_b); }

template <unsigned S>
std::string toJS(FixedHash<S> const& _h) {
  return toHexPrefixed(_h.ref());
}

template <unsigned N>
std::string toJS(
    boost::multiprecision::number<boost::multiprecision::cpp_int_backend<
        N, N, boost::multiprecision::unsigned_magnitude, boost::multiprecision::unchecked, void>> const& _n) {
  std::string h = toHex(toCompactBigEndian(_n, 1));
  // remove first 0, if it is necessary;
  std::string res = h[0] != '0' ? h : h.substr(1);
  return "0x" + res;
}

inline std::string toJS(bytes const& _n, std::size_t _padding = 0) {
  if (_n.empty()) return "0x";

  bytes n = _n;
  n.resize(std::max<unsigned>(n.size(), _padding));
  return toHexPrefixed(n);
}

template <unsigned T>
std::string toJS(SecureFixedHash<T> const& _i) {
  std::stringstream stream;
  stream << "0x" << _i.makeInsecure().hex();
  return stream.str();
}

template <typename T>
std::string toJS(T const& _i) {
  std::stringstream stream;
  stream << "0x" << std::hex << _i;
  return stream.str();
}

enum class OnFailed { InterpretRaw, Empty, Throw };

/// Convert string to byte array. Input parameter is hex, optionally prefixed by
/// "0x". Returns empty array if invalid input.
bytes jsToBytes(std::string const& _s, OnFailed _f = OnFailed::Empty);

template <unsigned N>
FixedHash<N> jsToFixed(std::string const& _s) {
  if (_s.substr(0, 2) == "0x")
    // Hex
    return FixedHash<N>(_s.substr(2 + std::max<unsigned>(N * 2, _s.size() - 2) - N * 2));
  else if (_s.find_first_not_of("0123456789") == std::string::npos)
    // Decimal
    return (typename FixedHash<N>::Arith)(_s);
  else
    // Binary
    return FixedHash<N>();  // FAIL
}

template <unsigned N>
boost::multiprecision::number<boost::multiprecision::cpp_int_backend<
    N * 8, N * 8, boost::multiprecision::unsigned_magnitude, boost::multiprecision::unchecked, void>>
jsToInt(std::string const& _s) {
  if (_s.substr(0, 2) == "0x")
    // Hex
    return fromBigEndian<boost::multiprecision::number<boost::multiprecision::cpp_int_backend<
        N * 8, N * 8, boost::multiprecision::unsigned_magnitude, boost::multiprecision::unchecked, void>>>(
        fromHex(_s.substr(2)));
  else if (_s.find_first_not_of("0123456789") == std::string::npos)
    // Decimal
    return boost::multiprecision::number<boost::multiprecision::cpp_int_backend<
        N * 8, N * 8, boost::multiprecision::unsigned_magnitude, boost::multiprecision::unchecked, void>>(_s);
  else
    // Binary
    return 0;  // FAIL
}

inline u256 jsToU256(std::string const& _s) { return jsToInt<32>(_s); }

inline uint64_t jsToInt(std::string const& _s, int base = 16) { return std::stoull(_s, 0, base); }

inline std::string jsToDecimal(std::string const& _s) { return toString(jsToU256(_s)); }

inline uint64_t getUInt(const Json::Value& v) {
  if (v.isString()) {
    return dev::jsToInt(v.asString());
  }

  return v.asUInt64();
}

}  // namespace dev
