// Copyright 2016-2021 Tyler Gilbert and Stratify Labs, Inc; see LICENSE.md

#include <fcntl.h>
#include <unistd.h>

#include <crypto/Random.hpp>
#include <crypto/Sha256.hpp>

#include "sos/Auth.hpp"

#if defined __link
#define OPEN() ::link_open(m_driver, "/dev/auth", LINK_O_RDWR)
#define CLOSE() ::link_close(m_driver, m_fd)
#define IOCTL(REQUEST, CTL) ::link_ioctl(m_driver, m_fd, REQUEST, CTL)
#else
#define OPEN() ::open("/dev/auth", O_RDWR)
#define CLOSE() ::close(m_fd)
#define IOCTL(REQUEST, CTL) ::ioctl(m_fd, REQUEST, CTL)
#endif

using namespace sos;

Auth::Token::Token(const var::StringView token) {
  populate(var::Data::from_string(token));
}

Auth::Token::Token(var::View token) { populate(token); }

void Auth::Token::populate(var::View data) {
  memset(&m_auth_token, 0, sizeof(m_auth_token));
  u32 size
    = data.size() > sizeof(m_auth_token) ? sizeof(m_auth_token) : data.size();
  memcpy(&m_auth_token, data.to_const_void(), size);
}

Auth::Auth(const var::StringView path FSAPI_LINK_DECLARE_DRIVER_LAST)
  : m_file(
    path.is_empty() ? "/dev/auth" : path,
    fs::OpenMode::read_write() FSAPI_LINK_INHERIT_DRIVER_LAST) {}

bool Auth::authenticate(var::View key) {
  crypto::Random random;
  crypto::Sha256 hash;

  var::Data random_data(Token::size());
  random.randomize(random_data);

  Token token = start(Token(random_data));

  auth_key_token_t key_token;
  auth_key_token_t reverse_key_token;

  key_token.key = Token(key).auth_token();
  key_token.token = token.auth_token();
  reverse_key_token.key = key_token.token;
  reverse_key_token.token = key_token.key;

  // do SHA256 calcs
  Token validation_token
    = finish(Token(crypto::Sha256().update(var::View(key_token)).output()));

  // hash.start();
  // hash << var::View(reverse_key_token);
  // hash.finish();

  const crypto::Sha256::Hash reverse_key_token_hash
    = crypto::Sha256().update(var::View(reverse_key_token)).output();

  // hash output should match validation token
  if (
    var::View(reverse_key_token_hash)
    == var::View(validation_token.auth_token().data)) {
    return true;
  }

  return false;
}

Auth::Token Auth::start(const Token &token) {
  auth_token_t result = token.auth_token();
  if (m_file.ioctl(I_AUTH_START, &result).is_error()) {
    return Token();
  }
  return Token(result);
}

Auth::Token Auth::finish(const Token &token) {
  auth_token_t result = token.auth_token();
  if (m_file.ioctl(I_AUTH_FINISH, &result).is_error()) {
    return Token();
  }
  return Token(result);
}
