// Copyright 2016-2021 Tyler Gilbert and Stratify Labs, Inc; see LICENSE.md

#include <fcntl.h>
#include <unistd.h>

#include <crypto/Random.hpp>
#include <crypto/Sha256.hpp>
#include <fs/ViewFile.hpp>

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


  var::Array<u8, Token::size()> random_data;
  random.randomize(random_data);

  auto token_key = Token(key);
  auto token_out = Token(random_data);

  auto random_token = start(token_out);

  if( var::View(random_token.auth_token()).truncate(16) != var::View(random_data).truncate(16) ){
    //first 16 bytes should stay the same
    return false;
  }

  const auth_key_token_t hash_out_input = {
    .key = token_key.auth_token(),
    .token = random_token.auth_token()};

  const auth_key_token_t hash_in_input = {
    .key = random_token.auth_token(),
    .token = token_key.auth_token()};

  const auto hash_out = Token(
    crypto::Sha256::get_hash(fs::ViewFile(var::View(hash_out_input))));

  // do SHA256 calcs
  const auto hash_in = finish(hash_out);

  const auto hash_in_expected = Token(
    crypto::Sha256::get_hash(fs::ViewFile(var::View(hash_in_input))));

  if( hash_in == hash_in_expected ){
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
