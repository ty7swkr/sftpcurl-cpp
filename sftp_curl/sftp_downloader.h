/*
 * sftp_downloader.h
 *
 *  Created on: 2026. 1. 6.
 *      Author: tys
 */

#pragma once

#include "sftp_curl_base.h"

#include <system_error>
#include <cstdio>

/**
 * @brief SFTP 다운로드를 수행하는 클래스
 *
 * SFTP 서버의 파일을 로컬 파일 시스템으로 다운로드합니다.
 * 파일 경로를 지정하면 내부적으로 파일을 열어서 쓰기 작업을 수행합니다.
 * 다운로드 완료 후 자동으로 파일을 닫습니다.
 *
 * @note 다운로드할 로컬 경로는 쓰기 권한이 있어야 합니다.
 * @note thread-safe하지 않습니다.
 *
 * @throws SFTPCurl::exception::code(), what() CURL 에러 발생 시 CURL 에러 코드와 CURL 에러 메시지
 *
 * @example
 * @code
 * try
 * {
 *   SFTPDownloader downloader;
 *   downloader.username("user").password("pass")
 *             .url("sftp://server.com/remote_file.txt")
 *             .download("./local_file.txt")
 *             .perform();
 * }
 * catch (const SFTPCurl::exception &e)
 * {
 *   std::cerr << "다운로드 실패: " << e.code() << " " << e.what() << std::endl;
 * }
 * @endcode
 *
 * @example 타임아웃과 함께 사용
 * @code
 * try
 * {
 *   SFTPDownloader downloader;
 *   downloader.username("user").password("pass")
 *             .connect_timeout(10)
 *             .timeout(300)
 *             .url("sftp://server.com/large_file.zip")
 *             .download("./large_file.zip") // 다운로드 받을 파일
 *             .perform() // 실행
 * }
 * catch (const SFTPCurl::exception &e)
 * {
 *   std::cerr << "다운로드 실패: " << e.code() << " " << e.what() << std::endl;
 * }
 * @endcode
 *
 * @see SFTPUploader
 */
class SFTPDownloader : public SFTPCurlTemplate<SFTPDownloader>
{
public:
  /** @brief CURLOPT_UPLOAD을 0으로 설정하여 다운로드 모드로 초기화합니다. */
  SFTPDownloader() : SFTPCurlTemplate<SFTPDownloader>() { curl_setopt(CURLOPT_UPLOAD, 0L); }

  /**
   * @brief curl 핸들을 새로 생성합니다. 기존의 curl 핸들은 해제됩니다.
   * @note 공통 reset에 더해 CURLOPT_UPLOAD를 재설정합니다.
   */
  SFTPDownloader &reset        ();

  /** @brief curl 핸들은 유지하지만 연결은 끊깁니다. CURLOPT_UPLOAD를 재설정합니다. */
  SFTPDownloader &reset_options();

  /** @brief SFTP 서버의 파일을 로컬에 저장합니다. perform() 호출 스레드에서 실행됩니다. */
  Performer download(const std::string &local_filename);
  /**
   * @brief 콜백 함수를 통해 스트림 방식으로 다운로드합니다. perform() 호출 스레드에서 실행됩니다.
   * @param stream_func data: 수신된 데이터 버퍼, data_size: 버퍼 크기.
   *                    반환값: 처리한 바이트 수.
   */
  Performer download(const std::function<size_t(void *data, size_t data_size)> &stream_func);

protected:
  /** @brief libcurl WRITEFUNCTION 콜백. stream_func_를 호출하여 수신된 데이터를 처리합니다. */
  static size_t write_callback(void *ptr, size_t size, size_t nmemb, void *userdata);

protected:
  std::function<size_t(void *data, size_t data_size)> stream_func_ = nullptr;
};

inline SFTPDownloader &
SFTPDownloader::reset()
{
  SFTPCurlTemplate::reset();
  curl_setopt(CURLOPT_UPLOAD, 0L);
  return *this;
}

inline SFTPDownloader &
SFTPDownloader::reset_options()
{
  SFTPCurlTemplate::reset_options();
  curl_setopt(CURLOPT_UPLOAD, 0L);
  return *this;
}

inline SFTPDownloader::Performer
SFTPDownloader::download(const std::string &local_filename)
{
  FILE *file = fopen(local_filename.c_str(), "wb");
  if (file == nullptr)
  {
    auto error_no = errno;
    throw_ec(CURLE_WRITE_ERROR, errno_string(error_no) + " [" + url_ + "], " + local_filename);
  }

  try
  {
    set_option(CURLOPT_UPLOAD,        0L);
    set_option(CURLOPT_DIRLISTONLY,   0L);

    set_option(CURLOPT_WRITEFUNCTION, (void*)NULL);
    set_option(CURLOPT_WRITEDATA,     file);
  }
  catch (...)
  {
    fclose(file);
    throw;
  }

  return Performer(*this,
                   " [" + url_ + "], " + local_filename,
                   [file](){ fclose(file); });
}

inline SFTPDownloader::Performer
SFTPDownloader::download(const std::function<size_t(void *data, size_t data_size)> &stream_func)
{
  stream_func_ = stream_func;

  set_option(CURLOPT_UPLOAD,        0L);
  set_option(CURLOPT_DIRLISTONLY,   0L);

  set_option(CURLOPT_WRITEFUNCTION, write_callback);
  set_option(CURLOPT_WRITEDATA,     this);

  return Performer(*this, ", stream_func");
}

inline size_t
SFTPDownloader::write_callback(void *ptr, size_t size, size_t nmemb, void *userdata)
{
  SFTPDownloader *client = reinterpret_cast<SFTPDownloader *>(userdata);
  if (client == nullptr)
    return 0;

  if (client->stream_func_ == nullptr)
    return 0;

  return client->stream_func_(ptr, size * nmemb);
}
