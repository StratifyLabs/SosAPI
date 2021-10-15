// Copyright 2016-2021 Tyler Gilbert and Stratify Labs, Inc; see LICENSE.md

#include <fcntl.h>
#include <unistd.h>

#include <crypto/Aes.hpp>
#include <crypto/Random.hpp>
#include <crypto/Sha256.hpp>

#include <fs/Path.hpp>
#include <fs/ViewFile.hpp>
#include <printer/Printer.hpp>

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
Printer &operator<<(Printer &printer, const sos::Auth::SignatureInfo &a) {
  return printer.key("size", var::NumberString(a.size()))
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

  if (
    var::View(random_token.auth_token()).truncate(16)
    != var::View(random_data).truncate(16)) {
    // first 16 bytes should stay the same
    return false;
  }

  const auth_key_token_t hash_out_input
    = {.key = token_key.auth_token(), .token = random_token.auth_token()};

  const auth_key_token_t hash_in_input
    = {.key = random_token.auth_token(), .token = token_key.auth_token()};

  const auto hash_out
    = Token(crypto::Sha256::get_hash(fs::ViewFile(var::View(hash_out_input))));

  // do SHA256 calcs
  const auto hash_in = finish(hash_out);

  const auto hash_in_expected
    = Token(crypto::Sha256::get_hash(fs::ViewFile(var::View(hash_in_input))));

  if (hash_in == hash_in_expected) {
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

Auth::SignatureInfo Auth::get_signature_info(const fs::FileObject &file) {

  fs::File::LocationScope ls(file);

  if (file.size() < sizeof(auth_signature_marker_t)) {
    return SignatureInfo();
  }

  const auto signature = get_signature(file);

  if (signature.is_valid() == false) {
    return SignatureInfo();
  }

  const size_t hash_size = file.size() - sizeof(auth_signature_marker_t);

  auto hash = [](const fs::FileObject &file, size_t hash_size) {
    crypto::Sha256 result;
    fs::File::LocationScope ls(file);
    file.seek(0);
    fs::NullFile().write(file, result, fs::File::Write().set_size(hash_size));
    return result.output();
  }(file, hash_size);

  return SignatureInfo().set_hash(hash).set_signature(signature).set_size(
    hash_size);
}

crypto::Dsa::Signature Auth::get_signature(const fs::FileObject &file) {
  if (file.size() < sizeof(auth_signature_marker_t)) {
    return crypto::Dsa::Signature();
  }

  fs::File::LocationScope ls(file);

  const size_t marker_location = file.size() - sizeof(auth_signature_marker_t);
  auth_signature_marker_t signature = {};
  file.seek(marker_location).read(var::View(signature));

  if (
    (signature.start == AUTH_SIGNATURE_MARKER_START)
    && (signature.next == AUTH_SIGNATURE_MARKER_NEXT)
    && (signature.size == AUTH_SIGNATURE_MARKER_SIZE + 512)) {
    return crypto::Dsa::Signature(var::View(signature.signature.data));
  } else {
    return crypto::Dsa::Signature();
  }
}

crypto::Dsa::Signature
Auth::sign(const fs::FileObject &file, const crypto::Dsa &dsa) {
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

#if defined __link
void Auth::create_secure_file(const CreateSecureFile &options) {
  const var::PathString input_path = options.input_path();
  const var::PathString secure_path = options.output_path();

  const auto input_hash = crypto::Sha256::get_hash(fs::File(input_path));

  fs::File padded_file
    = fs::File(input_path, fs::OpenMode::append_write_only());
  const u32 original_size = padded_file.size();
  const u32 padding_required = 16 - (original_size % 16);

  for (const auto i : api::Index(padding_required)) {
    MCU_UNUSED_ARGUMENT(i);
    char c = options.padding_character();
    padded_file.write(var::View(c));
  }

  // hash and encrypt the file
  crypto::Aes::Key encryption_key;

  if (options.key().is_empty() == false) {
    if (options.is_remove_key() == false) {
      API_RETURN_ASSIGN_ERROR(
        "you must create a secure archive when using a custom key",
        EINVAL);
    }
    const auto option_key = crypto::Aes::Key::from_string(options.key());
    encryption_key.set_key(option_key.get_key256());
  }

  const auto key_to_write = options.is_remove_key()
                              ? crypto::Aes::Key().nullify().key256()
                              : encryption_key.key256();

  // writes the key and IV to the file
  fs::File(fs::File::IsOverwrite::yes, secure_path)
    .write(var::View(secure_file_version))
    .write(var::View(original_size))
    .write(key_to_write)
    .write(encryption_key.initialization_vector())
    .write(input_hash)
    .write(
      fs::File(input_path),
      crypto::AesCbcEncrypter()
        .set_initialization_vector(encryption_key.initialization_vector())
        .set_key256(encryption_key.key256()),
      fs::File::Write().set_progress_callback(options.progress_callback()));
}

void Auth::create_plain_file(const CreatePlainFile &options) {


  fs::File source = fs::File(options.input_path());

  u32 version = 0;
  source.read(var::View(version));

  u32 original_size = 0;

  if (version == secure_file_version) {
    source.read(var::View(original_size));
  } else {
    // older versions just started with original file size
    original_size = version;
  }

  crypto::Aes::Key256 key_256;
  crypto::Aes::InitializationVector iv;
  crypto::Sha256::Hash input_hash;
  source.read(key_256).read(iv);

  if( version == secure_file_version){
    source.read(input_hash);
  }

  if (source.is_error()) {
    API_RETURN_ASSIGN_ERROR("failed to read metadata from source file", EINVAL);
  }

  if (options.key().is_empty() == false) {
    key_256
      = crypto::Aes::Key(crypto::Aes::Key::Construct().set_key(options.key()))
          .key256();
  } else {
    if (crypto::Aes::Key(key_256).is_key_null()) {
      // error
      API_RETURN_ASSIGN_ERROR(
        "no key was provided, but a key is required",
        EINVAL);
    }
  }

  fs::File output_file
    = fs::File(fs::File::IsOverwrite::yes, options.output_path())
        .write(
          source,
          crypto::AesCbcDecrypter().set_initialization_vector(iv).set_key256(
            key_256),
          fs::File::Write().set_progress_callback(options.progress_callback()))
        .move();

  // need File::truncate(path, size)
  truncate(var::PathString(options.output_path()).cstring(), original_size);

  if (version == secure_file_version) {
    const auto check_input_hash
      = crypto::Sha256::get_hash(fs::File(options.output_path()));
    if (var::View(check_input_hash) != var::View(input_hash)) {
      API_RETURN_ASSIGN_ERROR(
        "failed to decrypt file and recover original Sha256 hash -- password "
        "is probably wrong",
        EINVAL);
    }
  }
}
#endif
