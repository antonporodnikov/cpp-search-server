#include "string_processing.h"

using namespace std;

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }
    return words;
}

vector<string_view> SplitIntoWordsSTRV(string_view str) {
    vector<string_view> result;
    while (true) {
        const auto space = str.find(' ');
        result.push_back(str.substr(0, space));
        if (space == str.npos) {
            break;
        }
        else {
            str.remove_prefix(space + 1);
        }
    }
    return result;
}
