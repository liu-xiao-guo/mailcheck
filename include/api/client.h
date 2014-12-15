#ifndef API_CLIENT_H_
#define API_CLIENT_H_

#include <api/config.h>

#include <atomic>
#include <deque>
#include <map>
#include <string>

namespace api {

/**
 * Provide a nice way to access the HTTP API.
 *
 * We don't want our scope's code to be mixed together with HTTP and JSON handling.
 */
class Client {
public:

    /**
     * Result struct
     */
    struct Result {
        std::string uri;
        std::string title;
        std::string art;
        std::string subtitle;
        std::string description;
    };

    /**
     * A list of weather information
     */
    typedef std::deque<Result> ResultList;

    Client(Config::Ptr config);

    virtual ~Client() = default;

    /**
     * Search for results
     */
    virtual ResultList search(const std::string &query);

    /**
     * Cancel any pending queries (this method can be called from a different thread)
     */
    virtual void cancel();

    virtual Config::Ptr config();

protected:

    /**
     * Hang onto the configuration information
     */
    Config::Ptr config_;

    /**
     * Thread-safe cancelled flag
     */
    std::atomic<bool> cancelled_;
};

}

#endif // API_CLIENT_H_

