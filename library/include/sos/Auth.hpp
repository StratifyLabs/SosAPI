// Copyright 2016-2021 Tyler Gilbert and Stratify Labs, Inc; see LICENSE.md

#ifndef SOSAPI_AUTH_HPP
#define SOSAPI_AUTH_HPP

#if SOS_API_USE_CRYPTO_API

#include <sos/dev/auth.h>
#include <sos/link.h>

#include <fs/File.hpp>
#include <var/String.hpp>

#include <crypto/Ecc.hpp>
#include <crypto/Sha256.hpp>

#include "Link.hpp"


namespace sos {

/*! \brief Auth Class
 * \details This class is used to authenticate
 * the calling thread to the OS and gain root privileges.
 *
 * This is similar to using `sudo`.
 *
 *
 * ```msc
 * Caller->System: Sends 128-bit Random Number
 * Note right of System: Appends 128-bits
 * System->Caller: 256-bit Random Number
 * Caller->System: SHA256(Secret Key, Random Number)
 * Note right of System: Validates token
 * System->Caller: SHA256(Random Number, Secret Key)
 * ```
 *
 */
class Auth : public api::ExecutionContext {
public:
  class Token {
  public:
    Token() { m_auth_token = {0}; }
    explicit Token(const auth_token_t &auth_token) : m_auth_token(auth_token) {}
    explicit Token(const var::StringView token);
    explicit Token(var::View token);

    bool is_valid() const {
      for (u8 i = 0; i < sizeof(auth_token_t); i++) {
        if (m_auth_token.data[i] != 0) {
          return true;
        }
      }
      return false;
    }

    bool operator==(const Token &a) const {
      return memcmp(&a.m_auth_token, &m_auth_token, sizeof(auth_token_t)) == 0;
    }

    var::String to_string() const {
      var::String result;
      for (u8 i = 0; i < sizeof(auth_token_t); i++) {
        result += var::String().format("%02x", m_auth_token.data[i]);
      }
      return result;
    }

    const auth_token_t &auth_token() const { return m_auth_token; }

    static constexpr u32 size() { return sizeof(auth_token_t); }

  private:
    void populate(var::View data);
    auth_token_t m_auth_token;
  };

  Auth() {}
  Auth(var::StringView device FSAPI_LINK_DECLARE_DRIVER_NULLPTR_LAST);

  Auth(const Auth &a) = delete;
  Auth &operator=(const Auth &a) = delete;

  Auth(Auth &&a) { std::swap(m_file, a.m_file); }
  Auth &operator=(Auth &&a) {
    std::swap(m_file, a.m_file);
    return *this;
  }

  bool authenticate(var::View key);

  Token start(const Token &token);
  Token finish(const Token &token);

  crypto::Dsa::PublicKey get_public_key() const;

  bool is_valid() const { return is_success(); }


  class SignatureInfo {
    API_AC(SignatureInfo, crypto::Dsa::Signature, signature);
    API_AC(SignatureInfo, crypto::Sha256::Hash, hash);
    API_AF(SignatureInfo, size_t, size, 0);
  };

  static SignatureInfo get_signature_info(const fs::FileObject & file);
  static crypto::Dsa::Signature get_signature(const fs::FileObject & file);
  static crypto::DigitalSignatureAlgorithm::Signature sign(const fs::FileObject & file, const crypto::Dsa & dsa);
  static void append(const fs::FileObject & file, const crypto::Dsa::Signature & signature);
  static bool verify(const fs::FileObject & file, const crypto::Dsa::PublicKey & public_key);
  static constexpr size_t signature_marker_size = sizeof(auth_signature_marker_t);


#if defined __link
  class CreateSecureFile {
    API_AC(CreateSecureFile, var::StringView, input_path);
    API_AC(CreateSecureFile, var::StringView, output_path);
    API_AC(CreateSecureFile, var::StringView, key);
    API_AF(CreateSecureFile, char, padding_character, '\n');
    API_AB(CreateSecureFile, remove_key, true);
    API_AF(CreateSecureFile, const api::ProgressCallback*, progress_callback, nullptr);
  };

  static void create_secure_file(const CreateSecureFile & options);

  class CreatePlainFile {
    API_AC(CreatePlainFile, var::StringView, input_path);
    API_AC(CreatePlainFile, var::StringView, output_path);
    API_AC(CreatePlainFile, var::StringView, key);
    API_AF(CreatePlainFile, const api::ProgressCallback*, progress_callback, nullptr);
  };
  static void create_plain_file(const CreatePlainFile & options);
#endif

private:
  static constexpr u32 secure_file_version = 0x00000100;
#if defined __link
  API_AF(Auth, link_transport_mdriver_t *, driver, nullptr);
  Link::File m_file;
#else
  fs::File m_file;
#endif
};

} // namespace sys

namespace printer {
Printer &
operator<<(Printer &printer, const sos::Auth::SignatureInfo &a);
} // namespace printer

#endif

#endif // SOSAPI_AUTH_HPP
