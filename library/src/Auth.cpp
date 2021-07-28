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

namespace printer {
class Printer;
Printer &operator<<(Printer &printer, const sos::Auth::SignatureInfo &a) {
  return printer
    .key("hash", var::View(a.hash()).to_string<var::GeneralString>())
    .key("signature", a.signature().to_string());
}
} // namespace printer

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

crypto::Dsa::PublicKey Auth::get_public_key() const {
  auth_public_key_t key = {};
  m_file.ioctl(I_AUTH_GET_PUBLIC_KEY, &key);
  return crypto::Dsa::PublicKey(var::View(key));
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

Auth::SignatureInfo
Auth::get_signature_info(const fs::FileObject &file) {

  fs::File::LocationScope ls(file);

  if (file.size() < sizeof(auth_signature_marker_t)) {
    return SignatureInfo();
  }
  const size_t hash_size
    = file.size() - sizeof(auth_signature_marker_t);

  auto hash = [](const fs::FileObject &file, size_t hash_size) {
    crypto::Sha256 result;
    fs::File::LocationScope ls(file);
    file.seek(0);
    fs::NullFile().write(file, result, fs::File::Write().set_size(hash_size));
    return result.output();
  }(file, hash_size);

  return SignatureInfo().set_hash(hash).set_signature(get_signature(file));
}

crypto::Dsa::Signature
Auth::get_signature(const fs::FileObject &file) {
  if (file.size() < sizeof(auth_signature_marker_t)) {
    return crypto::Dsa::Signature();
  }

  fs::File::LocationScope ls(file);

  const size_t marker_location
    = file.size() - sizeof(auth_signature_marker_t);
  auth_signature_marker_t signature;
  file.seek(marker_location).read(var::View(signature).fill(0));

  if (
    (signature.start == AUTH_SIGNATURE_MARKER_START)
    && (signature.next == AUTH_SIGNATURE_MARKER_NEXT)
    && (signature.size == AUTH_SIGNATURE_MARKER_SIZE + 512)) {
    return crypto::Dsa::Signature(var::View(signature.signature.data));
  } else {
    return crypto::Dsa::Signature();
  }
}

crypto::Dsa::Signature Auth::sign(const fs::FileObject & file, const crypto::Dsa & dsa){
  fs::File::LocationScope ls(file);
  const auto hash = crypto::Sha256::get_hash(file.seek(0));
  const auto signature = dsa.sign(hash);
  append(file, signature);
  return signature;
}

void Auth::append(
  const fs::FileObject &file,
  const crypto::Dsa::Signature &signature) {

  fs::File::LocationScope ls(file);

  auth_signature_marker_t marker = {
    .start = AUTH_SIGNATURE_MARKER_START,
    .next = AUTH_SIGNATURE_MARKER_NEXT,
    .size = AUTH_SIGNATURE_MARKER_SIZE + 512};

  var::View(marker.signature.data).copy(signature.data());
  file.seek(0, fs::File::Whence::end).write(var::View(marker));
}

bool Auth::verify(
  const fs::FileObject &file,
  const crypto::Dsa::PublicKey &public_key) {
  // hash the file up to the marker
  fs::File::LocationScope ls(file);

  if (file.size() < sizeof(auth_signature_marker_t)) {
    return false;
  }

  const auto signature_info = get_signature_info(file);

  return crypto::Dsa(crypto::Dsa::KeyPair().set_public_key(public_key))
    .verify(signature_info.signature(), signature_info.hash());
}
