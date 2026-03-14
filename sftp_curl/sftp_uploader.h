/*
 * sftp_uploader.h
 *
 *  Created on: 2025. 8. 16.
 *      Author: tys
 */

#pragma once

#include "sftp_curl_base.h"

#if __cplusplus >= 201703L
#include <filesystem>
#endif

#include <cstdio>
#include <sys/stat.h>

/**
 * @brief SFTP 업로드를 수행하는 클래스
 *
 * 로컬 파일 시스템의 파일을 SFTP 서버로 업로드합니다.
 * 파일 경로를 지정하면 내부적으로 파일을 열어서 읽기 작업을 수행합니다.
 * 업로드 완료 후 자동으로 파일을 닫습니다.
 *
 * @note 업로드할 파일은 읽기 권한이 있어야 하며,
 *       upload() 실행 중에는 파일이 다른 프로세스에서 수정되지 않아야 합니다.
 * @note thread-safe하지 않습니다.
 *
 * @throws SFTPCurl::exception::code(), what() CURL 에러 발생 시 CURL 에러 코드와 CURL 에러 메시지
 *
 * @example
 * @code
 * try
 * {
 *   SFTPUploader uploader;
 *   uploader.username("user").password("pass")
 *           .url("sftp://server.com/remote_file.txt")
 *           .upload("./local_file.txt")
 *           .perform();
 * }
 * catch (const SFTPCurl::exception &e)
 * {
 *   std::cerr << "업로드 실패: " << e.code() << " " << e.what() << std::endl;
 * }
 * @endcode
 *
 * @example 타임아웃과 함께 사용
 * @code
 * try
 * {
 *   SFTPUploader uploader;
 *   uploader.username("user").password("pass")
 *           .connect_timeout(10)
 *           .timeout(300)
 *           .url("sftp://server.com/large_file.zip") // 업로드 위치
 *           .upload("./large_file.zip")
 *           .perform();
 * }
 * catch (const SFTPCurl::exception &e)
 * {
 *   std::cerr << "업로드 실패: " << e.code() << " " << e.what() << std::endl;
 * }
 * @endcode
 *
 * @see SFTPUploader
 */
class SFTPUploader : public SFTPCurlTemplate<SFTPUploader>
{
public:
  /** @brief CURLOPT_UPLOAD을 1로 설정하여 업로드 모드로 초기화합니다. */
  SFTPUploader() : SFTPCurlTemplate<SFTPUploader>() { curl_setopt(CURLOPT_UPLOAD, 1L); }

  /**
   * @brief curl 핸들을 새로 생성합니다. 기존의 curl 핸들은 해제됩니다.
   * @note 공통 reset에 더해 CURLOPT_UPLOAD를 재설정합니다.
   */
  SFTPUploader &reset        ();
  /** @brief curl 핸들은 유지하지만 연결은 끊깁니다. CURLOPT_UPLOAD를 재설정합니다. */
  SFTPUploader &reset_options();

  /** @brief 로컬 파일을 SFTP 서버로 업로드합니다. perform() 호출 스레드에서 실행됩니다. */
  Performer upload(const std::string &filename);
  /**
   * @brief 콜백 함수를 통해 스트림 방식으로 업로드합니다. perform() 호출 스레드에서 실행됩니다.
   * @param stream_func target: curl이 제공하는 버퍼, target_size: 버퍼 크기.
   *                    반환값: 실제로 채운 바이트 수. 0 반환 시 전송 완료(EOF).
   */
  Performer upload(const std::function<size_t(void *target, size_t target_size)> &stream_func);

protected:
  /** @brief libcurl READFUNCTION 콜백. stream_func_를 호출하여 업로드할 데이터를 채웁니다. */
  static size_t read_callback(void *ptr, size_t size, size_t nmemb, void *userdata);

protected:
  std::function<size_t(void *target, size_t target_size)> stream_func_ = nullptr;
};

inline SFTPUploader &
SFTPUploader::reset()
{
  SFTPCurlTemplate::reset();
  curl_setopt(CURLOPT_UPLOAD, 1L);
  return *this;
}

inline SFTPUploader &
SFTPUploader::reset_options()
{
  SFTPCurlTemplate::reset_options();
  curl_setopt(CURLOPT_UPLOAD, 1L);
  return *this;
}

inline SFTPUploader::Performer
SFTPUploader::upload(const std::string &local_filename)
{
  // 파일 열기
  FILE *file = fopen(local_filename.c_str(), "rb");
  if (file == nullptr)
  {
    auto error_no = errno;
    throw_ec(CURLE_READ_ERROR, errno_string(error_no) + " [" + url_ + "], " + local_filename);
  }

  try
  {
#if __cplusplus >= 201703L
    // 파일 크기 구하기
    std::error_code error_code;
    auto file_size = std::filesystem::file_size(local_filename, error_code);
    if (error_code)
      throw_ec(CURLE_READ_ERROR, error_code.message() + " [" + url_ + "], " + local_filename);
#else
    // 파일 크기 구하기
    struct stat file_info;
    if (stat(local_filename.c_str(), &file_info) != 0)
    {
      auto error_no = errno;
      throw_ec(CURLE_READ_ERROR, errno_string(error_no) + " [" + url_ + "], " + local_filename);
    }

    auto file_size = file_info.st_size;
#endif

    // CURL 옵션 설정
    set_option(CURLOPT_UPLOAD,            1L);
    set_option(CURLOPT_DIRLISTONLY,       0L);

    set_option(CURLOPT_READFUNCTION,      (void*)NULL);
    set_option(CURLOPT_READDATA,          file);
    set_option(CURLOPT_INFILESIZE_LARGE,  static_cast<curl_off_t>(file_size));
  }
  catch (...)
  {
    fclose(file);
    throw;
  }

  return Performer(*this, " [" + url_ + "], " + local_filename, [file](){ fclose(file); });
}

inline SFTPUploader::Performer
SFTPUploader::upload(const std::function<size_t(void *target, size_t target_size)> &stream_func)
{
  stream_func_ = stream_func;

  set_option(CURLOPT_UPLOAD,            1L);
  set_option(CURLOPT_DIRLISTONLY,       0L);

  set_option(CURLOPT_READFUNCTION,      read_callback);
  set_option(CURLOPT_READDATA,          this);
  set_option(CURLOPT_INFILESIZE_LARGE,  -1);

  return Performer(*this, ", stream_func");
}

inline size_t
SFTPUploader::read_callback(void *ptr, size_t size, size_t nmemb, void *userdata)
{
  SFTPUploader *client = reinterpret_cast<SFTPUploader *>(userdata);
  if (client == nullptr)
    return 0;

  if (client->stream_func_ == nullptr)
    return CURL_READFUNC_ABORT;

  return client->stream_func_(ptr, size*nmemb);
}


