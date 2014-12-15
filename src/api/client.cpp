#include <api/client.h>


using namespace api;
using namespace std;

Client::Client(Config::Ptr config) :
    config_(config), cancelled_(false) {
}


Client::ResultList Client::search(const string &query) {
    ResultList results;

    {
        Result result;
        result.uri = "uri";
        result.title = query;
        result.art = "art.png";
        result.subtitle = "subtitle";
        result.description = "description";
        results.emplace_back(result);
    }
    {
        Result result;
        result.uri = "uri2";
        result.title = query;
        result.art = "art2.png";
        result.subtitle = "subtitle2";
        result.description = "description2";
        results.emplace_back(result);
    }

    return results;
}

void Client::cancel() {
    cancelled_ = true;
}

Config::Ptr Client::config() {
    return config_;
}

