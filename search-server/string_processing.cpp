#include "string_processing.h"

using namespace std;

vector<string_view> SplitIntoWords(string_view text) {

    vector<string_view> result;
    text.remove_prefix(min(text.find_first_not_of(" "), text.size()));
    while (!text.empty()) {
        size_t pos = text.find(" ");
        result.push_back(text.substr(0, pos));

        if (pos == text.npos) {
            text.remove_prefix(text.size());
        }
        else {
            text.remove_prefix(pos);
            text.remove_prefix(min(text.find_first_not_of(" "), text.size()));
        }
    }
    return result;
}