/*
 * sfpt_curl_base.h
 *
 *  Created on: 2025. 8. 16.
 *      Author: tys
 */

#pragma once

#include <curl/curl.h>

#include <memory>
#include <string>
#include <functional>
#include <set>
#include <sstream>

#include <errno.h>
#include <string.h>

/**
 * @brief SFTP 공통 기반 클래스
 *
 * exception, ScopeExit, 에러 처리 유틸리티를 제공합니다.
 * 직접 사용하지 않으며 SFTPCurlTemplate을 통해 상속됩니다.
 */
class SFTPCurlBase
{
public:
  /** @brief CURL 에러 코드와 메시지를 담는 예외 클래스 */
  class exception : public std::exception
  {
  public:
    exception(CURLcode code, const std::string &message)
    : code_(code), message_(message) {}

    exception(CURLcode code, std::string &&message)
    : code_(code), message_(std::move(message)) {}

    const CURLcode &code() const noexcept { return code_; }
    const char *    what() const noexcept override { return message_.c_str(); }

  private:
    CURLcode    code_;
    std::string message_;
  };

protected:
  /** @brief 스코프 종료 시 등록된 함수를 자동 실행하는 RAII 가드 */
  class ScopeExit
  {
  public:
    ScopeExit() = delete;
    ScopeExit(const ScopeExit &) = delete;

    explicit ScopeExit(const std::function<void()> &exit_func)
    : exit_func_(exit_func) {}

    ~ScopeExit()
    {
      if (cancel_    == true   ) return;
      if (exit_func_ == nullptr) return;
      exit_func_();
    }

    void cancel() noexcept { cancel_ = true; }

  private:
    std::function<void()> exit_func_ = nullptr;
    bool cancel_  = false;
  };

  /** @brief errno 코드를 "메시지(코드번호)" 형식의 문자열로 변환합니다. */
  std::string errno_string(int error_no)
  {
    char error_chars[256];
    char *error_string = strerror_r(error_no, error_chars, sizeof(error_chars));
    error_chars[sizeof(error_chars) - 1] = '\0'; // 명시적 널 종료 보장
    return std::string(error_string)+"(" + std::to_string(error_no) + ")";
  };

  /** @brief CURL 에러 코드로 exception을 발생시킵니다. */
  static void throw_ec(CURLcode code, const std::string &context)
  { throw SFTPCurlBase::exception(code, context); }
};

/**
 * @brief SFTP 클라이언트를 위한 CRTP 베이스 클래스
 *
 * 이 클래스는 CRTP(Curiously Recurring Template Pattern)를 사용하여
 * SFTP 연결 설정과 관련된 공통 기능을 제공합니다.
 * 메서드 체이닝을 통해 사용하거나 혹은 일반 함수 호출처럼 사용할 수 있습니다.
 *
 * @tparam T 파생 클래스 타입 (CRTP 패턴)
 *
 * @note 이 클래스는 직접 인스턴스화할 수 없으며, 파생 클래스를 통해 사용해야 합니다.
 * @note thread-safe하지 않습니다.
 *
 * @throws SFTPCurl::exception::code(), what() CURL 에러 발생 시 CURL 에러 코드와 CURL 에러 메시지
 *
 * @see SFTPUploader
 * @see SFTPDownloader
 */
template<typename T>
class SFTPCurlTemplate : protected SFTPCurlBase
{
public:
  /** @brief curl 핸들을 초기화합니다. 초기화 실패 시 exception을 발생시킵니다. */
  SFTPCurlTemplate(bool verify_host = false) : curl_(curl_easy_init(), curl_easy_cleanup)
  {
    if (curl_ == nullptr)
      throw_ec(CURLE_FAILED_INIT, curl_easy_strerror(CURLE_FAILED_INIT));

    this->verify_host(verify_host);
  }
  virtual ~SFTPCurlTemplate() {}

  /** @brief 현재 설정된 URL, 사용자명, 비밀번호를 반환합니다. */
  const std::string &url     () const noexcept { return url_; }
  const std::string &username() const noexcept { return username_; }
  const std::string &password() const noexcept { return password_; }

  /** @brief curl 핸들을 새로 생성합니다. 기존의 curl 핸들은 해제됩니다. */
  T &reset          () { curl_.reset(curl_easy_init()); return static_cast<T&>(*this); }

  /** @brief curl 핸들은 유지하지만 연결은 끊깁니다. */
  T &reset_options  () { curl_easy_reset(curl_.get()); url_.clear(); username_.clear(); password_.clear(); return static_cast<T&>(*this); }

  /** @brief 실행 중 상세 로그를 표준 출력으로 출력합니다. */
  T &verbose        (bool enable);

  /** @brief 연결을 시도하는 타임아웃 시간을 설정합니다. */
  T &connect_timeout(const long long    &seconds);

  /** @brief 실제 수행 타임아웃 시간을 설정합니다. 수행 시간이 seconds를 넘으면 exception이 발생합니다. */
  T &timeout        (const long long    &seconds);

  /** @brief SFTP 서버의 URL을 설정합니다. sftp://로 시작해야 합니다. */
  T &url            (const std::string  &url);

  /** @brief SFTP 서버의 포트를 설정합니다. 기본값은 22입니다. */
  T &port           (uint16_t port);

  /** @brief 사용자 인증 정보를 설정합니다. */
  T &userpass       (const std::string  &username, const std::string &password);
  T &username       (const std::string  &username);
  T &password       (const std::string  &password);

  /** @brief SSH 키 파일 및 피어 검증을 설정합니다. */
  T &private_key    (const std::string  &keyfile);
  T &public_key     (const std::string  &keyfile);
  T &verify_peer    (bool enable);

  /**
   * @brief 호스트 키 검증 여부를 설정합니다.
   * @note false(기본값): ~/.ssh/known_hosts에 등록되지 않은 호스트도 접근 가능합니다.
   * @note true: ~/.ssh/known_hosts에 등록되지 않은 호스트 접근 시 오류가 발생합니다.
   */
  T &verify_host    (bool enable);
  /** @brief SSH known_hosts 파일 경로를 설정합니다. */
  T &known_hosts    (const std::string  &hostfile);

  /** @brief CURL 옵션을 직접 설정합니다. 실패 시 exception을 발생시킵니다. */
  template<typename P>
  T &set_option(CURLoption option, P parameter);

  /** @brief SFTP 서버의 파일 목록을 가져옵니다. */
  virtual std::set<std::string> list();

  /**
   * @brief SFTP 서버에 파일이 존재하는지 확인합니다.
   * @return 파일이 존재하면 true, 존재하지 않으면 false를 반환합니다.
   */
  virtual bool exists();

  /** @brief perform() 호출을 지연 실행하는 프록시 객체 */
  struct Performer
  {
    Performer(T &sftp,
              const std::string           &throw_context,
              const std::function<void()> &cleanup = nullptr) noexcept
    : sftp_(sftp), throw_context_(throw_context), cleanup_(cleanup) {}

    void perform() { sftp_.perform(throw_context_, cleanup_); }

  private:
    std::string throw_context_;
    std::function<void()> cleanup_ = nullptr;
    T &sftp_;
  };

protected:
  /** @brief list() 에서 사용하는 libcurl WRITEFUNCTION 콜백. 수신된 라인을 set에 삽입합니다. */
  static size_t list_callback(void *contents, size_t size, size_t nmemb, void *userdata)
  {
    std::set<std::string> *files = reinterpret_cast<std::set<std::string> *>(userdata);
    if (files == nullptr)
      return CURL_READFUNC_ABORT;

    std::istringstream stream(std::string(static_cast<char*>(contents), size * nmemb));

    std::string line;
    while (std::getline(stream, line))
    {
      if (line.empty() == true)
        continue;
      files->insert(line);
    }

    return size * nmemb;
  }

protected:
  /** @brief curl_easy_perform()을 실행하고, 완료 후 cleanup을 호출합니다. 실패 시 exception을 발생시킵니다. */
  void perform(const std::string           &throw_context,
               const std::function<void()> &cleanup = nullptr)
  {
    ScopeExit scope([&](){ if (cleanup != nullptr) { cleanup(); } });

    auto res = curl_easy_perform(curl_.get());

    if (res != CURLE_OK)
      throw_ec(res, curl_easy_strerror(res) + throw_context);
  }

protected:
  /** @brief curl_easy_setopt() 래퍼. 실패해도 exception을 발생시키지 않습니다. */
  template<typename P>
  CURLcode curl_setopt(CURLoption option, P parameter) noexcept;

  std::string url_;
  std::string username_;
  std::string password_;

protected:
  std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl_;
};

template<typename T>
template<typename P> T &
SFTPCurlTemplate<T>::set_option(CURLoption option, P parameter)
{
  auto res = curl_setopt(option, parameter);
  if (res != CURLE_OK)
    throw_ec(res, curl_easy_strerror(res));

  return static_cast<T&>(*this);
}

template<typename T>
template<typename P> CURLcode
SFTPCurlTemplate<T>::curl_setopt(CURLoption option, P parameter) noexcept
{
  return curl_easy_setopt(curl_.get(), option, parameter);
}

template<typename T> std::set<std::string>
SFTPCurlTemplate<T>::list()
{
  set_option(CURLOPT_UPLOAD,        0L);
  set_option(CURLOPT_DIRLISTONLY,   1L);

  std::set<std::string> files;
  set_option(CURLOPT_WRITEFUNCTION, list_callback);
  set_option(CURLOPT_WRITEDATA,     &files);

  auto url = url_;
  ScopeExit scope([&](){ url_ = url; curl_setopt(CURLOPT_URL, url.c_str()); });

  if (url_.empty() == false)
    if (url_.back() != '/')
      this->url(url_ += '/');

  auto res = curl_easy_perform(curl_.get());
  if (res != CURLE_OK)
    throw_ec(res, curl_easy_strerror(res) + std::string(" [") + url_ + "]");

  set_option(CURLOPT_WRITEFUNCTION, (void*)NULL);
  set_option(CURLOPT_WRITEDATA,     (void*)NULL);
  set_option(CURLOPT_READFUNCTION,  (void*)NULL);
  set_option(CURLOPT_READDATA,      (void*)NULL);

  return files;
}

template<typename T> bool
SFTPCurlTemplate<T>::exists()
{
  if (url_.back() == '/')
    throw_ec(CURLE_BAD_FUNCTION_ARGUMENT,
             curl_easy_strerror(CURLE_BAD_FUNCTION_ARGUMENT) + std::string(" [") + url_ + "]");

  auto pos = url_.rfind('/');
  if (pos == std::string::npos)
    throw_ec(CURLE_BAD_FUNCTION_ARGUMENT,
             curl_easy_strerror(CURLE_BAD_FUNCTION_ARGUMENT) + std::string(" [") + url_ + "]");

  auto file = url_.substr(pos+1);
  if (file.length() == 0)
    throw_ec(CURLE_BAD_FUNCTION_ARGUMENT,
             curl_easy_strerror(CURLE_BAD_FUNCTION_ARGUMENT) + std::string(" [") + url_ + "]");

  if (url_.substr(0, pos).find("sftp://") == std::string::npos)
    throw_ec(CURLE_BAD_FUNCTION_ARGUMENT,
             curl_easy_strerror(CURLE_BAD_FUNCTION_ARGUMENT) + std::string(" [") + url_ + "]");

  auto url = url_;
  ScopeExit scope([&](){ url_ = url; curl_setopt(CURLOPT_URL, url.c_str()); });

  this->url(url_.substr(0, pos));
  return this->list().count(file) > 0;
}

template<typename T> T &
SFTPCurlTemplate<T>::url(const std::string &url)
{
  constexpr size_t prefix_len = sizeof("sftp://") - 1;
  if (url.length() < prefix_len)
    throw_ec(CURLE_UNSUPPORTED_PROTOCOL,
             curl_easy_strerror(CURLE_UNSUPPORTED_PROTOCOL) + std::string(" [") + url + "]");

  if (url.compare(0, prefix_len, "sftp://") != 0)
    throw_ec(CURLE_UNSUPPORTED_PROTOCOL,
             curl_easy_strerror(CURLE_UNSUPPORTED_PROTOCOL) + std::string(" [") + url + "]");

  url_ = url;
  return set_option(CURLOPT_URL, url_.c_str());
}

template<typename T>
T &SFTPCurlTemplate<T>::verbose(bool enable)
{
  return set_option(CURLOPT_VERBOSE, enable ? 1L : 0L);
}

template<typename T>
T &SFTPCurlTemplate<T>::connect_timeout(const long long &seconds)
{
  return set_option(CURLOPT_CONNECTTIMEOUT, seconds);
}

template<typename T>
T &SFTPCurlTemplate<T>::timeout(const long long &seconds)
{
  return set_option(CURLOPT_TIMEOUT, seconds);
}

template<typename T> T &
SFTPCurlTemplate<T>::port(uint16_t port)
{
  return set_option(CURLOPT_PORT, port);
}

template<typename T> T &
SFTPCurlTemplate<T>::userpass(const std::string &username, const std::string &password)
{
  this->username(username);
  return this->password(password);
}

template<typename T> T &
SFTPCurlTemplate<T>::username(const std::string &username)
{
  username_ = username;
  return set_option(CURLOPT_USERNAME, username_.c_str());
}

template<typename T> T &
SFTPCurlTemplate<T>::password(const std::string &password)
{
  password_ = password;
  return set_option(CURLOPT_PASSWORD, password_.c_str());
}

template<typename T> T &
SFTPCurlTemplate<T>::private_key(const std::string &keyfile)
{
  return set_option(CURLOPT_SSH_PRIVATE_KEYFILE, keyfile.c_str());
}

template<typename T> T &
SFTPCurlTemplate<T>::public_key(const std::string &keyfile)
{
  return set_option(CURLOPT_SSH_PUBLIC_KEYFILE, keyfile.c_str());
}

template<typename T> T &
SFTPCurlTemplate<T>::verify_peer(bool enable)
{
  return set_option(CURLOPT_SSL_VERIFYPEER, enable ? 1L : 0L);
}

template<typename T> T &
SFTPCurlTemplate<T>::verify_host(bool enable)
{
  return set_option(CURLOPT_SSL_VERIFYHOST, enable ? 2L : 0L);
}

template<typename T> T &
SFTPCurlTemplate<T>::known_hosts(const std::string &hostfile)
{
  return set_option(CURLOPT_SSH_KNOWNHOSTS, hostfile.c_str());
}

