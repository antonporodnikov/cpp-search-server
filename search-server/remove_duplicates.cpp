#include "remove_duplicates.h"

using namespace std;

void RemoveDuplicates(SearchServer& search_server) {
    vector<int> duplicates;
    set<set<string>> unique_words;
    for (const int document_id : search_server) {
        set<string> words_to_check;
        for (auto& [word, freq] : search_server.GetWordFrequencies(document_id)) {
            words_to_check.insert(word);
        }
        if (unique_words.count(words_to_check)) {
            duplicates.push_back(document_id);
        } else {
            unique_words.insert(words_to_check);
        }
        words_to_check.clear();
    }
    for (int duplicate : duplicates) {
        cout << "Found duplicate document id "s << duplicate << endl;
        search_server.RemoveDocument(duplicate);
    }
}
