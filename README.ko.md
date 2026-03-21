# sftpcurl-cpp

libcurl 기반의 간단한 C++ SFTP 라이브러리

- libcurl-7.61.1 이상
- C++11 이상
- header only

## 예제

### 파일 업/다운로드

```cpp
#include <sftp_curl/sftp_curl.h>

...........

try
{
  SFTPUploader uploader;
  uploader.username("user")
          .password("pass")
          .url("sftp://server.com/remote/path/file.txt")
          .upload("./local_file.txt")
          .perform();
}
catch (const SFTPCurlBase::exception &e)
{
  std::cerr << e.what() << std::endl;
}
```

```cpp
#include <sftp_curl/sftp_curl.h>

...........

try
{
  SFTPDownloader downloader;
  downloader.username("user")
            .password("pass")
            .url("sftp://server.com/remote/path/file.txt")
            .download("./local_file.txt")
            .perform();
}
catch (const SFTPCurlBase::exception &e)
{
  std::cerr << e.what() << std::endl;
}
```

### 스트림 업로드

파일 경로 대신 콜백 함수로 데이터를 직접 처리.

```cpp
#include <sftp_curl/sftp_curl.h>

...........

// 업로드: target 버퍼에 데이터를 채워 반환, 0 반환 시 전송 완료
try
{
  std::string data = "hello world";
  size_t offset = 0;
  uploader.username("user").password("pass")
          .url("sftp://server.com/remote/path/file.txt")
          .upload([&](void *target, size_t target_size) -> size_t
          {
            // 이 람다 함수는 perform()시의 스레드에서 실행됨.
            size_t remain  = data.size() - offset;
            size_t to_copy = remain < target_size ? remain : target_size;
            memcpy(target, data.data() + offset, to_copy);
            offset += to_copy;
            return to_copy;
          })
          .perform();
}
catch (const SFTPCurlBase::exception &e)
{
  std::cerr << e.what() << std::endl;
}
```

### 스트림 다운로드

```cpp
#include <sftp_curl/sftp_curl.h>

...........

// 다운로드: 수신 데이터를 메모리에 누적
try
{
  std::string result;
  downloader.username("user").password("pass")
            .url("sftp://server.com/remote/path/file.txt")
            .download([&](void *data, size_t data_size) -> size_t
            {
              // 이 람다 함수는 perform()시의 스레드에서 실행됨.
              result.append(static_cast<char *>(data), data_size);
              return data_size;
            })
            .perform();
}
catch (const SFTPCurlBase::exception &e)
{
  std::cerr << e.what() << std::endl;
}
```

### 파일 목록

```cpp
#include <sftp_curl/sftp_curl.h>

...........

try
{
  SFTPDownloader downloader;
  auto files = downloader.username("user")
                         .password("pass")
                         .url("sftp://server.com/remote/path/")
                         .list();

  for (auto &file : files)
    std::cout << file << std::endl;
}
catch (const SFTPCurlBase::exception &e)
{
  std::cerr << e.what() << std::endl;
}
```

### 파일 존재 여부
```cpp
#include <sftp_curl/sftp_curl.h>

...........

try
{
  SFTPDownloader downloader;
  // 파일 존재 여부
  bool found = downloader.url("sftp://server.com/remote/path/file.txt").exists();
}
catch (const SFTPCurlBase::exception &e)
{
  std::cerr << e.what() << std::endl;
}
```

### 타임아웃 설정

```cpp
#include <sftp_curl/sftp_curl.h>

...........

try
{
  SFTPUploader uploader;
  uploader.username("user")
          .password("pass")
          .connect_timeout(10)   // 연결 타임아웃 (초)
          .timeout(300)          // 전송 타임아웃 (초)
          .url("sftp://server.com/remote/path/file.zip")
          .upload("./file.zip")
          .perform();
}
catch (const SFTPCurlBase::exception &e)
{
  std::cerr << e.what() << std::endl;
}
```

---
<br>

## API

### 공통 API (SFTPCurlTemplate)

SFTPUploader와 SFTPDownloader가 공통으로 상속하는 메서드들이에요.
모든 설정 메서드는 자기 자신의 참조를 반환하므로 메서드 체이닝이 가능해요.

| 메서드 | 설명 |
|---|---|
| `url(const std::string &url)` | SFTP 서버 URL 설정 (`sftp://`로 시작) |
| `port(uint16_t port)` | 포트 설정 (기본값 22) |
| `username(const std::string &username)` | 사용자명 설정 |
| `password(const std::string &password)` | 비밀번호 설정 |
| `url()` | 현재 설정된 URL 반환 |
| `username()` | 현재 설정된 사용자명 반환 |
| `password()` | 현재 설정된 비밀번호 반환 |
| `userpass(const std::string &username, const std::string &password)` | 사용자명/비밀번호 동시 설정 |
| `connect_timeout(const long long &seconds)` | 연결 타임아웃 설정 (초) |
| `timeout(const long long &seconds)` | 수행 타임아웃 설정 (초). 초과 시 exception 발생 |
| `verbose(bool enable)` | 상세 로그 출력 활성화/비활성화 |
| `private_key(const std::string &keyfile)` | SSH 개인 키 파일 경로 설정 |
| `public_key(const std::string &keyfile)` | SSH 공개 키 파일 경로 설정 |
| `verify_peer(bool enable)` | SSL 피어 검증 활성화/비활성화 |
| `verify_host(bool enable)` | 호스트 키 검증 활성화/비활성화. `false`(기본값): known_hosts 미등록 호스트도 접근 가능 |
| `known_hosts(const std::string &hostfile)` | SSH known_hosts 파일 경로 설정 |
| `set_option(CURLoption option, P parameter)` | CURL 옵션 직접 설정. 실패 시 exception 발생 |
| `reset()` | curl 핸들을 새로 생성. 기존 핸들 해제 |
| `reset_options()` | curl 핸들 유지, 연결 끊기 및 옵션 초기화 |
| `list()` | SFTP 서버의 파일 목록 조회. `std::set<std::string>` 반환 |
| `exists()` | SFTP 서버에 파일 존재 여부 확인. `bool` 반환 |

<br>

**예외**

| 클래스 | 설명 |
|---|---|
| `SFTPCurlBase::exception` | CURL 에러 코드와 메시지를 담는 예외 클래스 |
| `exception::code()` | `CURLcode` 반환 |
| `exception::what()` | 에러 메시지 반환 |

<br>

### SFTPDownloader

| 메서드 | 설명 |
|---|---|
| `download(const std::string &local_filename)` | SFTP 서버 파일을 로컬에 저장. `Performer` 반환 |
| `download(const std::function<size_t(void *data, size_t data_size)> &stream_func)` | 콜백을 통한 스트림 다운로드. `Performer` 반환 |
| `reset()` | curl 핸들 재생성 (다운로드 모드 유지) |
| `reset_options()` | 옵션 초기화 (다운로드 모드 유지) |

`Performer::perform()` 을 호출하면 실제 전송 실행.

<br>

### SFTPUploader

| 메서드 | 설명 |
|---|---|
| `upload(const std::string &filename)` | 로컬 파일을 SFTP 서버로 업로드. `Performer` 반환 |
| `upload(const std::function<size_t(void *target, size_t target_size)> &stream_func)` | 콜백을 통한 스트림 업로드. 0 반환 시 전송 완료. `Performer` 반환 |
| `reset()` | curl 핸들 재생성 (업로드 모드 유지) |
| `reset_options()` | 옵션 초기화 (업로드 모드 유지) |

`Performer::perform()` 을 호출하면 실제 전송 실행.
