# sftpcurl-cpp

[한국어](README.ko.md)

A simple C++ SFTP library based on libcurl

- libcurl 7.61.1 or higher
- C++11 or higher
- Header only

## Examples

### File Upload/Download

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

### Stream Upload

Process data directly via callback functions instead of file paths.

```cpp
#include <sftp_curl/sftp_curl.h>

...........

// Upload: Fill the target buffer with data and return the size. Return 0 to signal completion.
try
{
  std::string data = "hello world";
  size_t offset = 0;
  uploader.username("user").password("pass")
          .url("sftp://server.com/remote/path/file.txt")
          .upload([&](void *target, size_t target_size) -> size_t
          {
            // This lambda runs on the thread that calls perform().
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

### Stream Download

```cpp
#include <sftp_curl/sftp_curl.h>

...........

// Download: Accumulate received data in memory
try
{
  std::string result;
  downloader.username("user").password("pass")
            .url("sftp://server.com/remote/path/file.txt")
            .download([&](void *data, size_t data_size) -> size_t
            {
              // This lambda runs on the thread that calls perform().
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

### File Listing

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

### File Existence Check
```cpp
#include <sftp_curl/sftp_curl.h>

...........

try
{
  SFTPDownloader downloader;
  // Check if file exists
  bool found = downloader.url("sftp://server.com/remote/path/file.txt").exists();
}
catch (const SFTPCurlBase::exception &e)
{
  std::cerr << e.what() << std::endl;
}
```

### Timeout Settings

```cpp
#include <sftp_curl/sftp_curl.h>

...........

try
{
  SFTPUploader uploader;
  uploader.username("user")
          .password("pass")
          .connect_timeout(10)   // Connection timeout (seconds)
          .timeout(300)          // Transfer timeout (seconds)
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

### Common API (SFTPCurlTemplate)

These are methods inherited by both SFTPUploader and SFTPDownloader.
All setter methods return a reference to themselves, enabling method chaining.

| Method | Description |
|---|---|
| `url(const std::string &url)` | Set SFTP server URL (must start with `sftp://`) |
| `port(uint16_t port)` | Set port (default: 22) |
| `username(const std::string &username)` | Set username |
| `password(const std::string &password)` | Set password |
| `url()` | Return the currently set URL |
| `username()` | Return the currently set username |
| `password()` | Return the currently set password |
| `userpass(const std::string &username, const std::string &password)` | Set username and password at once |
| `connect_timeout(const long long &seconds)` | Set connection timeout (seconds) |
| `timeout(const long long &seconds)` | Set operation timeout (seconds). Throws exception on timeout |
| `verbose(bool enable)` | Enable/disable verbose logging |
| `private_key(const std::string &keyfile)` | Set SSH private key file path |
| `public_key(const std::string &keyfile)` | Set SSH public key file path |
| `verify_peer(bool enable)` | Enable/disable SSL peer verification |
| `verify_host(bool enable)` | Enable/disable host key verification. `false` (default): allows access to hosts not registered in known_hosts |
| `known_hosts(const std::string &hostfile)` | Set SSH known_hosts file path |
| `set_option(CURLoption option, P parameter)` | Set a CURL option directly. Throws exception on failure |
| `reset()` | Create a new curl handle. Releases the existing handle |
| `reset_options()` | Keep the curl handle, disconnect and reset options |
| `list()` | List files on the SFTP server. Returns `std::set<std::string>` |
| `exists()` | Check if a file exists on the SFTP server. Returns `bool` |

<br>

**Exceptions**

| Class | Description |
|---|---|
| `SFTPCurlBase::exception` | Exception class containing CURL error code and message |
| `exception::code()` | Returns `CURLcode` |
| `exception::what()` | Returns error message |

<br>

### SFTPDownloader

| Method | Description |
|---|---|
| `download(const std::string &local_filename)` | Download a file from the SFTP server to local storage. Returns `Performer` |
| `download(const std::function<size_t(void *data, size_t data_size)> &stream_func)` | Stream download via callback. Returns `Performer` |
| `reset()` | Recreate curl handle (retains download mode) |
| `reset_options()` | Reset options (retains download mode) |

Call `Performer::perform()` to execute the actual transfer.

<br>

### SFTPUploader

| Method | Description |
|---|---|
| `upload(const std::string &filename)` | Upload a local file to the SFTP server. Returns `Performer` |
| `upload(const std::function<size_t(void *target, size_t target_size)> &stream_func)` | Stream upload via callback. Return 0 to signal completion. Returns `Performer` |
| `reset()` | Recreate curl handle (retains upload mode) |
| `reset_options()` | Reset options (retains upload mode) |

Call `Performer::perform()` to execute the actual transfer.
