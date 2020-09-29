#include "chaining.h"

#include <curl/curl.h>
#include <curl/easy.h>

#include <sstream>

#include <faabric/util/config.h>
#include <faabric/util/func.h>
#include <faabric/util/json.h>
#include <faabric/util/logging.h>

namespace faabric::util {
size_t dataCallback(void* ptr, size_t size, size_t nmemb, void* stream)
{
    std::string data((const char*)ptr, (size_t)size * nmemb);
    *((std::stringstream*)stream) << data;
    return size * nmemb;
}

std::string postJsonFunctionCall(const std::string& host,
                                 int port,
                                 const faabric::Message& msg)
{
    std::string cleanedFuncName;
    if (msg.ispython()) {
        cleanedFuncName = "python";
    } else {
        cleanedFuncName = msg.function();
        std::replace(cleanedFuncName.begin(), cleanedFuncName.end(), '_', '-');
    }

    std::string url = "http://" + host + ":" + std::to_string(port);

    const std::shared_ptr<spdlog::logger>& logger = faabric::util::getLogger();
    const std::string funcStr = faabric::util::funcToString(msg, true);
    int callTimeoutMs = faabric::util::getSystemConfig().chainedCallTimeout;

    void* curl = curl_easy_init();

    std::stringstream out;
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, dataCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, callTimeoutMs);

    // Add header for knative calls. Ignored on other platforms
    // Note: need to replace underscores with hyphens
    std::string knativeHeader =
      "Host: faabric-" + cleanedFuncName + ".faabric.example.com";

    struct curl_slist* chunk = nullptr;
    chunk = curl_slist_append(chunk, "Content-type: application/json");
    chunk = curl_slist_append(chunk, knativeHeader.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

    // Add the message as JSON
    const std::string msgJson = faabric::util::messageToJson(msg);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, msgJson.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, -1L);

    logger->debug(
      "Posting function {} to {} ({})", funcStr, url, knativeHeader);
    logger->debug("Posted function JSON {}", msgJson);

    // Make the request
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || out.str().empty()) {
        std::string errMsg = "Chained call to " + url + " failed";
        throw ChainedCallFailedException(errMsg);
    }

    return out.str();
}
}
