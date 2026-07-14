
#pragma once

#include <string>
#include <vector>
#include <memory>

#include <curl/curl.h>


inline bool GetRemoteFile(const char* url, std::string& str, std::string& error,
                          long* responseCode = nullptr, const char* contentType = nullptr,
                          std::string request_type = "", const char* postData = nullptr,
                          std::vector<std::string> extraHeaders = std::vector<std::string>(),
                          std::string* signature = nullptr, int timeoutSec = 0,
                          bool fail_on_error = true, int postDataSize = 0) { return false; }

inline std::string GetAPIData(const char* url,
			      const char* request_type = "POST", const char *postData = nullptr,
			      std::vector<std::string> extraHeaders = std::vector<std::string>(),
			      const char* cookie = nullptr) { return std::string(); }
